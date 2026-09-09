#include "cli_manager.h"
#include "version.h"
#include "ble/ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "keymap/key_config_storage.h"
#include "wifi/wifi_manager.h"
#include "esp_wifi.h"
#include "USBCDC.h"
#include <Arduino.h>
#include <ArduinoJson.h>

extern key_mapper_engine_t g_key_engine;
extern USBCDC USBSerial;

static String s_input_buffer = "";

static void print_json(const JsonDocument& doc) {
    String out;
    serializeJson(doc, out);
    USBSerial.println(out);
}

static void handle_command(const String& line) {
    String cmd = line;
    cmd.trim();
    if (cmd.length() == 0) return;

    // Parse "cmd arg1 arg2 ..." style commands
    int first_space = cmd.indexOf(' ');
    String base = (first_space < 0) ? cmd : cmd.substring(0, first_space);
    base.trim();
    String args = (first_space < 0) ? "" : cmd.substring(first_space + 1);
    args.trim();

    if (base.equalsIgnoreCase("status") || base.equalsIgnoreCase("info")) {
        JsonDocument doc;
        doc["firmware"] = FIRMWARE_NAME;
        doc["version"] = FIRMWARE_VERSION;
        doc["target"] = HARDWARE_TARGET;
        doc["uptime_sec"] = millis() / 1000;
        doc["ble_state"] = (int)ble_remote_get_state();
        doc["frames_decoded"] = g_audio_pipeline.total_frames_decoded;
        doc["samples_pushed"] = g_audio_pipeline.total_samples_pushed;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["free_psram"] = ESP.getFreePsram();
        doc["ap_ip"] = wifi_manager_get_ap_ip();
        doc["sta_ip"] = wifi_manager_get_sta_ip();
        doc["sta_connected"] = wifi_manager_is_sta_connected();
        print_json(doc);
    }
    else if (base.equalsIgnoreCase("reconnect")) {
        USBSerial.println("{\"status\":\"reconnecting\"}");
        ble_remote_trigger_reconnect();
    }
    else if (base.equalsIgnoreCase("reset_keys")) {
        key_config_storage_reset_defaults(&g_key_engine);
        USBSerial.println("{\"status\":\"keymap_reset_to_defaults\"}");
    }
    else if (base.equalsIgnoreCase("wifi_scan")) {
        USBSerial.println(wifi_manager_scan_json());
    }
    else if (base.equalsIgnoreCase("wifi_sta")) {
        int sp = args.indexOf(' ');
        String ssid = (sp < 0) ? args : args.substring(0, sp);
        String pass = (sp < 0) ? "" : args.substring(sp + 1);
        ssid.trim();
        pass.trim();
        if (ssid.length() == 0) {
            USBSerial.println("{\"error\":\"empty_ssid\"}");
            return;
        }
        if (wifi_manager_save_sta_config(ssid, pass)) {
            USBSerial.println("{\"status\":\"ok\"}");
        } else {
            USBSerial.println("{\"error\":\"save_failed\"}");
        }
    }
    else if (base.equalsIgnoreCase("wifi_ap")) {
        if (wifi_manager_save_ap_config(args)) {
            USBSerial.println("{\"status\":\"ok\"}");
        } else {
            USBSerial.println("{\"error\":\"password_too_short\"}");
        }
    }
    else if (base.equalsIgnoreCase("wifi_off")) {
        // 调试用：彻底停掉 Wi-Fi（SoftAP+STA），把射频完全让给 BLE
        esp_wifi_disconnect();
        esp_wifi_stop();
        USBSerial.println("{\"status\":\"wifi_off\"}");
    }
    else if (base.equalsIgnoreCase("wifi_on")) {
        esp_wifi_start();
        USBSerial.println("{\"status\":\"wifi_on\"}");
    }
    else if (base.equalsIgnoreCase("ble_scan")) {
        USBSerial.println(ble_remote_scan_devices_json());
    }
    else if (base.equalsIgnoreCase("ble_connect")) {
        if (args.length() == 0) {
            USBSerial.println("{\"error\":\"empty_mac\"}");
            return;
        }
        if (ble_remote_connect_mac(args)) {
            USBSerial.println("{\"status\":\"connecting\"}");
        } else {
            USBSerial.println("{\"status\":\"failed\"}");
        }
    }
    else if (base.equalsIgnoreCase("ble_unpair")) {
        ble_remote_unpair();
        USBSerial.println("{\"status\":\"unpaired\"}");
    }
    else if (base.equalsIgnoreCase("ble_info")) {
        USBSerial.println(ble_remote_get_connected_info());
    }
    else if (base.equalsIgnoreCase("keymap_get")) {
        USBSerial.println(key_config_to_json(&g_key_engine));
    }
    else if (base.equalsIgnoreCase("keymap_set")) {
        if (args.length() == 0) {
            USBSerial.println("{\"error\":\"empty_json\"}");
            return;
        }
        if (key_config_from_json(&g_key_engine, args)) {
            key_config_storage_save(&g_key_engine);
            USBSerial.println("{\"status\":\"saved\"}");
        } else {
            USBSerial.println("{\"error\":\"invalid_keymap_format\"}");
        }
    }
    else if (base.equalsIgnoreCase("help")) {
        USBSerial.println("Commands:");
        USBSerial.println("  status              - System info & runtime statistics (JSON)");
        USBSerial.println("  info                - Same as status");
        USBSerial.println("  reconnect           - Trigger BLE remote re-scan");
        USBSerial.println("  reset_keys          - Reset key bindings to factory defaults");
        USBSerial.println("  wifi_scan           - Scan nearby Wi-Fi networks");
        USBSerial.println("  wifi_sta <ssid> [pass] - Save STA credentials and connect");
        USBSerial.println("  wifi_ap [pass]      - Set AP password (empty = open)");
        USBSerial.println("  wifi_off            - Stop Wi-Fi completely (BLE RF debug)");
        USBSerial.println("  wifi_on             - Restart Wi-Fi");
        USBSerial.println("  ble_scan            - Scan nearby BLE devices");
        USBSerial.println("  ble_connect <mac>     - Connect and bind a BLE remote");
        USBSerial.println("  ble_unpair          - Unpair current BLE remote");
        USBSerial.println("  ble_info            - Show bound/connected BLE info");
        USBSerial.println("  keymap_get          - Dump current keymap JSON");
        USBSerial.println("  keymap_set <json>   - Load and save keymap from JSON");
        USBSerial.println("  help                - Show available commands");
    }
    else {
        USBSerial.println("{\"error\":\"unknown_command\",\"hint\":\"type help\"}");
    }
}

extern "C" {

void cli_manager_init(void) {
    // keymap_set 的完整键位表 JSON 可达 10KB+，必须预留足够缓冲
    s_input_buffer.reserve(8192);
}

void cli_manager_task(void) {
    while (USBSerial.available() > 0) {
        char c = (char)USBSerial.read();
        if (c == '\r' || c == '\n') {
            if (s_input_buffer.length() > 0) {
                handle_command(s_input_buffer);
                s_input_buffer = "";
            }
        } else {
            s_input_buffer += c;
            // 防御无换行垃圾数据导致内存无限增长（正常 keymap_set 最大 ~14KB）
            if (s_input_buffer.length() > 32768) {
                s_input_buffer = "";
            }
        }
    }
}

} // extern "C"
