#include "web_server.h"
#include "web_ui.h"
#include "log/app_log.h"
#include "wifi/wifi_manager.h"
#include "version.h"
#include "ble/ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "keymap/key_config_storage.h"
#include <WebServer.h>
#include <ArduinoJson.h>

static WebServer s_server(80);
extern key_mapper_engine_t g_key_engine;

static void handle_root() {
    s_server.sendHeader("Connection", "close");
    s_server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handle_status() {
    JsonDocument doc;
    doc["firmware"] = FIRMWARE_NAME;
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime_sec"] = millis() / 1000;
    doc["ble_state"] = (int)ble_remote_get_state();
    doc["frames_decoded"] = g_audio_pipeline.total_frames_decoded;
    doc["samples_pushed"] = g_audio_pipeline.total_samples_pushed;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["ap_ip"] = wifi_manager_get_ap_ip();
    doc["sta_ip"] = wifi_manager_get_sta_ip();
    doc["sta_connected"] = wifi_manager_is_sta_connected();

    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

static void handle_logs() {
    String json = app_log_get_json();
    s_server.send(200, "application/json", json);
}

static void handle_logs_clear() {
    app_log_clear();
    s_server.send(200, "application/json", "{\"status\":\"cleared\"}");
}

static void handle_wifi_scan() {
    String json = wifi_manager_scan_json();
    s_server.send(200, "application/json", json);
}

static void handle_wifi_config() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
    if (err) {
        s_server.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";

    if (ssid.length() == 0) {
        s_server.send(400, "application/json", "{\"error\":\"empty_ssid\"}");
        return;
    }

    wifi_manager_save_sta_config(ssid, pass);
    s_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handle_keymap_get() {
    String json = key_config_to_json(&g_key_engine);
    s_server.send(200, "application/json", json);
}

static void handle_keymap_save() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    bool ok = key_config_from_json(&g_key_engine, s_server.arg("plain"));
    if (ok) {
        key_config_storage_save(&g_key_engine);
        s_server.send(200, "application/json", "{\"status\":\"saved\"}");
    } else {
        s_server.send(400, "application/json", "{\"error\":\"invalid_keymap_format\"}");
    }
}

static void handle_keymap_reset() {
    key_config_storage_reset_defaults(&g_key_engine);
    app_log("KEYMAP", "Reset keymap to factory defaults via Web API");
    s_server.send(200, "application/json", "{\"status\":\"reset_ok\"}");
}

static void handle_keymap_telemetry() {
    String json = key_telemetry_to_json(&g_key_engine);
    s_server.send(200, "application/json", json);
}

static void handle_ble_scan() {
    String json = ble_remote_scan_devices_json();
    s_server.send(200, "application/json", json);
}

static void handle_ble_connect() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
    if (err) {
        s_server.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    String mac = doc["mac"] | "";
    if (mac.length() == 0) {
        s_server.send(400, "application/json", "{\"error\":\"empty_mac\"}");
        return;
    }

    bool ok = ble_remote_connect_mac(mac);
    s_server.send(200, "application/json", ok ? "{\"status\":\"connected\"}" : "{\"status\":\"failed\"}");
}

static void handle_ble_unpair() {
    ble_remote_unpair();
    s_server.send(200, "application/json", "{\"status\":\"unpaired\"}");
}

static void handle_ble_info() {
    String json = ble_remote_get_connected_info();
    s_server.send(200, "application/json", json);
}

static void handle_ble_reconnect() {
    app_log("BLE", "Triggered manual reconnect scan via Web API");
    ble_remote_trigger_reconnect();
    s_server.send(200, "application/json", "{\"status\":\"reconnecting\"}");
}

static void handle_system_restart() {
    app_log("SYSTEM", "Rebooting ESP32 via Web API...");
    s_server.send(200, "application/json", "{\"status\":\"rebooting\"}");
    delay(500);
    ESP.restart();
}

static void handle_captive_portal() {
    String host = s_server.hostHeader();
    if (host != "192.168.4.1" && host != "remotemapper.local") {
        s_server.sendHeader("Location", "http://192.168.4.1/", true);
        s_server.send(302, "text/plain", "");
        return;
    }
    handle_root();
}

void web_server_init(void) {
    // API Routes
    s_server.on("/", HTTP_GET, handle_root);
    s_server.on("/api/status", HTTP_GET, handle_status);
    s_server.on("/api/logs", HTTP_GET, handle_logs);
    s_server.on("/api/logs/clear", HTTP_POST, handle_logs_clear);
    s_server.on("/api/wifi/scan", HTTP_GET, handle_wifi_scan);
    s_server.on("/api/wifi/config", HTTP_POST, handle_wifi_config);
    s_server.on("/api/keymap", HTTP_GET, handle_keymap_get);
    s_server.on("/api/keymap/save", HTTP_POST, handle_keymap_save);
    s_server.on("/api/keymap/reset", HTTP_POST, handle_keymap_reset);
    s_server.on("/api/keymap/telemetry", HTTP_GET, handle_keymap_telemetry);
    s_server.on("/api/ble/scan", HTTP_GET, handle_ble_scan);
    s_server.on("/api/ble/connect", HTTP_POST, handle_ble_connect);
    s_server.on("/api/ble/unpair", HTTP_POST, handle_ble_unpair);
    s_server.on("/api/ble/info", HTTP_GET, handle_ble_info);
    s_server.on("/api/ble/reconnect", HTTP_POST, handle_ble_reconnect);
    s_server.on("/api/system/restart", HTTP_POST, handle_system_restart);

    // Captive Portal probe redirects
    s_server.on("/generate_204", HTTP_GET, handle_captive_portal);
    s_server.on("/hotspot-detect.html", HTTP_GET, handle_captive_portal);
    s_server.on("/canonical.html", HTTP_GET, handle_captive_portal);
    s_server.on("/connecttest.txt", HTTP_GET, handle_captive_portal);
    s_server.onNotFound(handle_captive_portal);

    s_server.begin();
    app_log("WEB", "HTTP Web Server started on port 80");
}

void web_server_task(void) {
    s_server.handleClient();
}
