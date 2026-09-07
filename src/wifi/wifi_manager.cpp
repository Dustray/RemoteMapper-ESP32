#include "wifi_manager.h"
#include "log/app_log.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define DNS_PORT        53
#define MDNS_HOSTNAME   "remotemapper"

static DNSServer        s_dns_server;
static Preferences      s_prefs;
static IPAddress        s_ap_ip(192, 168, 4, 1);
static IPAddress        s_ap_netmask(255, 255, 255, 0);

static bool             s_sta_configured = false;
static uint32_t         s_last_sta_check = 0;

void wifi_manager_init(void) {
    s_prefs.begin("wifi_conf", false);
    String sta_ssid = s_prefs.getString("ssid", "");
    String sta_pass = s_prefs.getString("pass", "");
    String ap_pass  = s_prefs.getString("ap_pass", "");

    // Set Wi-Fi Mode
    WiFi.mode(WIFI_AP_STA);

    // 1. Configure and start AP Mode
    WiFi.softAPConfig(s_ap_ip, s_ap_ip, s_ap_netmask);
    if (ap_pass.length() >= 8) {
        WiFi.softAP(AP_SSID, ap_pass.c_str());
        app_log("WIFI", "AP Started: %s (WPA2-PSK, IP: 192.168.4.1)", AP_SSID);
    } else {
        WiFi.softAP(AP_SSID, "");
        app_log("WIFI", "AP Started: %s (Open Network, IP: 192.168.4.1)", AP_SSID);
    }

    // 2. Start Captive Portal DNS
    s_dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns_server.start(DNS_PORT, "*", s_ap_ip);

    // 3. Connect to Home Wi-Fi if saved
    if (sta_ssid.length() > 0) {
        s_sta_configured = true;
        app_log("WIFI", "Connecting to Home Wi-Fi: %s...", sta_ssid.c_str());
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    } else {
        app_log("WIFI", "No Home Wi-Fi configured, running in AP Setup mode");
    }

    // 4. Start mDNS
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        app_log("WIFI", "mDNS responder started: http://%s.local", MDNS_HOSTNAME);
    }
}

void wifi_manager_task(void) {
    s_dns_server.processNextRequest();

    uint32_t now = millis();
    if (s_sta_configured && (now - s_last_sta_check > 5000)) {
        s_last_sta_check = now;
        if (WiFi.status() == WL_CONNECTED) {
            static bool s_logged_connected = false;
            if (!s_logged_connected) {
                app_log("WIFI", "Connected to Home Wi-Fi! Local IP: %s", WiFi.localIP().toString().c_str());
                s_logged_connected = true;
            }
        }
    }
}

String wifi_manager_get_ap_ip(void) {
    return WiFi.softAPIP().toString();
}

String wifi_manager_get_sta_ip(void) {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "Disconnected";
}

bool wifi_manager_is_sta_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}

int8_t wifi_manager_get_sta_rssi(void) {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

String wifi_manager_scan_json(void) {
    app_log("WIFI", "Scanning for 2.4GHz Wi-Fi networks...");

    // Clean up any stale previous scan results to avoid stale/empty reads
    WiFi.scanDelete();

    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = WiFi.SSID(i);
        obj["rssi"] = WiFi.RSSI(i);
        obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    String out;
    serializeJson(doc, out);
    WiFi.scanDelete();
    return out;
}

bool wifi_manager_save_sta_config(const String& ssid, const String& password) {
    if (ssid.length() == 0) return false;

    s_prefs.putString("ssid", ssid);
    s_prefs.putString("pass", password);
    s_sta_configured = true;

    app_log("WIFI", "Saved new Wi-Fi credentials for: %s, connecting...", ssid.c_str());
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    return true;
}

String wifi_manager_get_ap_pass(void) {
    return s_prefs.getString("ap_pass", "");
}

bool wifi_manager_save_ap_config(const String& ap_password) {
    String p = ap_password;
    p.trim();
    if (p.length() > 0 && p.length() < 8) {
        return false;
    }
    s_prefs.putString("ap_pass", p);

    WiFi.softAPConfig(s_ap_ip, s_ap_ip, s_ap_netmask);
    if (p.length() >= 8) {
        WiFi.softAP(AP_SSID, p.c_str());
        app_log("WIFI", "AP reconfigured: %s (WPA2-PSK)", AP_SSID);
    } else {
        WiFi.softAP(AP_SSID, "");
        app_log("WIFI", "AP reconfigured: %s (Open Network)", AP_SSID);
    }
    return true;
}

