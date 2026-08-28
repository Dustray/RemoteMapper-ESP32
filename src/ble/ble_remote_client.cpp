#include "ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "usb/usb_composite.h"
#include "log/app_log.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>

static ble_remote_state_t              s_ble_state = BLE_STATE_DISCONNECTED;
static NimBLEClient*                   s_client = nullptr;
static NimBLERemoteCharacteristic*     s_char_cmd = nullptr;
static NimBLERemoteCharacteristic*     s_char_aud = nullptr;
static NimBLERemoteCharacteristic*     s_char_ctl = nullptr;

static Preferences                     s_ble_prefs;
static String                          s_bound_mac = "";
static String                          s_connected_name = "";
static String                          s_connected_mac = "";

static uint8_t                         s_session_id = 0;
static uint32_t                        s_last_audio_ms = 0;
static uint32_t                        s_last_extend_ms = 0;
static uint32_t                        s_last_scan_ms = 0;
static size_t                          s_frame_size = AUDIO_DEFAULT_FRAME_BYTES;

extern key_mapper_engine_t g_key_engine;

// Forward Declarations
static void start_scan();
static bool connect_to_remote(NimBLEAdvertisedDevice* advDevice);
static bool connect_to_mac_internal(const NimBLEAddress& address, const String& dev_name);

// Audio Notification Callback (ATVV Char 0x03)
static void on_audio_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (s_ble_state != BLE_STATE_TALKING || length == 0) return;
    s_last_audio_ms = millis();
    audio_pipeline_feed_adpcm(&g_audio_pipeline, pData, length);
}

// Control Notification Callback (ATVV Char 0x04)
static void on_ctl_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 1) return;
    uint8_t op = pData[0];

    // AUDIO_START with HTT reason: byte1 == 0x03
    if (op == 0x04 && length >= 2 && pData[1] == 0x03) {
        s_session_id = (length >= 4) ? pData[3] : 0;
        s_ble_state = BLE_STATE_TALKING;
        s_last_audio_ms = millis();
        s_last_extend_ms = millis();

        // Trigger Voice Hold action (hold hotkey & reset audio DSP)
        key_action_t act = { ACTION_VOICE_HOLD, DEFAULT_VOICE_MODIFIER, DEFAULT_VOICE_KEY, 0 };
        usb_hid_dispatch_action(&act);
        app_log("ATVV", ">>> Voice button PRESSED (session %d)", s_session_id);
    }
    // AUDIO_STOP / MIC_CLOSED / release op:
    else if (op == 0x00) {
        if (s_ble_state == BLE_STATE_TALKING) {
            s_ble_state = BLE_STATE_CONNECTED;
            key_action_t act = { ACTION_VOICE_RELEASE, DEFAULT_VOICE_MODIFIER, DEFAULT_VOICE_KEY, 0 };
            usb_hid_dispatch_action(&act);
            app_log("ATVV", "<<< Voice button RELEASED");

            // Re-arm remote HTT standby
            if (s_char_cmd != nullptr) {
                uint8_t cmd_open[] = { 0x0C, 0x00 };
                s_char_cmd->writeValue(cmd_open, sizeof(cmd_open), false);
            }
        }
    }
    // CAPS_RESP: op == 0x0B
    else if (op == 0x0B && length >= 7) {
        uint16_t ver = (pData[1] << 8) | pData[2];
        uint16_t fs = (pData[5] << 8) | pData[6];
        if (fs > 0) s_frame_size = fs;
        app_log("ATVV", "CAPS: ver=0x%04X, frame_size=%d", ver, (int)s_frame_size);
    }
    // AUDIO_SYNC: op == 0x0A
    else if (op == 0x0A && length >= 7) {
        int16_t pred = (int16_t)((pData[4] << 8) | pData[5]);
        int8_t step_idx = (int8_t)pData[6];
        audio_pipeline_sync(&g_audio_pipeline, pred, step_idx);
        app_log("ATVV", "SYNC: pred=%d, step=%d", pred, step_idx);
    }
}

// HOGP HID Report Notification Callback
static void on_hogp_report_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 1) return;
    uint8_t raw_key = pData[0];
    bool is_pressed = (length > 1) ? (pData[1] != 0) : (raw_key != 0);

    app_log("HOGP", "Key byte: 0x%02X (%s)", raw_key, is_pressed ? "DOWN" : "UP");
    key_engine_feed_key(&g_key_engine, raw_key, is_pressed, millis());
}

static bool is_target_remote(NimBLEAdvertisedDevice* dev) {
    String name = dev->getName().c_str();
    String addr = dev->getAddress().toString().c_str();
    addr.toLowerCase();

    // 1. Exact match with previously bound remote
    if (s_bound_mac.length() > 0 && addr.equalsIgnoreCase(s_bound_mac)) {
        return true;
    }

    // 2. Name contains Xiaomi / Remote keywords in Chinese & English
    if (name.indexOf("小米") >= 0 || name.indexOf("遥控") >= 0 ||
        name.indexOf("MI RC") >= 0 || name.indexOf("Xiaomi") >= 0 ||
        name.indexOf("Remote") >= 0 || name.indexOf("RC") >= 0) {
        return true;
    }

    // 3. Service UUID matches ATVV or HID
    if (dev->haveServiceUUID()) {
        if (dev->isAdvertisingService(NimBLEUUID(ATVV_SVC_UUID)) ||
            dev->isAdvertisingService(NimBLEUUID((uint16_t)HOGP_SVC_UUID))) {
            return true;
        }
    }

    // 4. Common Xiaomi Bluetooth OUI prefixes
    if (addr.startsWith("c0:5d:39") || addr.startsWith("64:90:c1") ||
        addr.startsWith("7c:49:eb") || addr.startsWith("50:ec:50") ||
        addr.startsWith("04:cf:8c") || addr.startsWith("28:6c:07") ||
        addr.startsWith("34:ce:00") || addr.startsWith("5c:c3:06")) {
        return true;
    }

    return false;
}

// Advertised Device Scan Callbacks
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        String name = advertisedDevice->getName().c_str();
        String addr = advertisedDevice->getAddress().toString().c_str();

        if (name.length() > 0) {
            app_log("BLE_SCAN", "Device: %s (%s, RSSI: %d)", name.c_str(), addr.c_str(), advertisedDevice->getRSSI());
        }

        if (is_target_remote(advertisedDevice)) {
            app_log("BLE", "Matching Target Remote: %s (%s), connecting...", name.c_str(), addr.c_str());
            NimBLEDevice::getScan()->stop();
            connect_to_remote(advertisedDevice);
        }
    }
};

// Client Connection Callbacks
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        app_log("BLE", "Remote GATT Connected!");
        s_ble_state = BLE_STATE_CONNECTED;
    }

    void onDisconnect(NimBLEClient* pClient) override {
        app_log("BLE", "Remote Disconnected");
        s_ble_state = BLE_STATE_DISCONNECTED;
        s_char_cmd = nullptr;
        s_char_aud = nullptr;
        s_char_ctl = nullptr;
    }
};

static void start_scan() {
    s_ble_state = BLE_STATE_SCANNING;
    s_last_scan_ms = millis();
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(BLE_SCAN_INTERVAL_MS);
    pScan->setWindow(BLE_SCAN_WINDOW_MS);
    pScan->start(6, false);
    app_log("BLE", "Scanning for Xiaomi Bluetooth Remote...");
}

static bool setup_services_and_handshake() {
    if (!s_client || !s_client->isConnected()) return false;

    // Discover ATVV Service
    NimBLERemoteService* pAtvvSvc = s_client->getService(NimBLEUUID(ATVV_SVC_UUID));
    if (pAtvvSvc) {
        s_char_cmd = pAtvvSvc->getCharacteristic(NimBLEUUID(ATVV_CHAR_CMD_UUID));
        s_char_aud = pAtvvSvc->getCharacteristic(NimBLEUUID(ATVV_CHAR_AUD_UUID));
        s_char_ctl = pAtvvSvc->getCharacteristic(NimBLEUUID(ATVV_CHAR_CTL_UUID));

        if (s_char_ctl && s_char_ctl->canNotify()) {
            s_char_ctl->subscribe(true, on_ctl_notify);
        }
        if (s_char_aud && s_char_aud->canNotify()) {
            s_char_aud->subscribe(true, on_audio_notify);
        }

        // Perform ATVV Handshake: GET_CAPS then MIC_OPEN
        if (s_char_cmd) {
            uint8_t cmd_caps[] = { 0x0A, 0x01, 0x00, 0x00, 0x03, 0x03 };
            s_char_cmd->writeValue(cmd_caps, sizeof(cmd_caps), false);
            delay(150);
            uint8_t cmd_open[] = { 0x0C, 0x00 };
            s_char_cmd->writeValue(cmd_open, sizeof(cmd_open), false);
            app_log("ATVV", "Handshake completed successfully (MIC_OPEN active)");
        }
    }

    // Discover HOGP Service
    NimBLERemoteService* pHogpSvc = s_client->getService(NimBLEUUID((uint16_t)HOGP_SVC_UUID));
    if (pHogpSvc) {
        NimBLERemoteCharacteristic* pReportChar = pHogpSvc->getCharacteristic(NimBLEUUID((uint16_t)HOGP_REPORT_CHAR_UUID));
        if (pReportChar && pReportChar->canNotify()) {
            pReportChar->subscribe(true, on_hogp_report_notify);
            app_log("HOGP", "Subscribed to HID Key Reports");
        }
    }

    s_ble_state = BLE_STATE_CONNECTED;
    return true;
}

static bool connect_to_remote(NimBLEAdvertisedDevice* advDevice) {
    s_ble_state = BLE_STATE_CONNECTING;
    if (s_client == nullptr) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(new ClientCallbacks(), false);
    }

    if (!s_client->connect(advDevice)) {
        app_log("BLE", "Connection Failed to %s", advDevice->getAddress().toString().c_str());
        s_ble_state = BLE_STATE_DISCONNECTED;
        return false;
    }

    s_connected_name = advDevice->getName().c_str();
    s_connected_mac = advDevice->getAddress().toString().c_str();
    if (s_connected_name.length() == 0) s_connected_name = "Xiaomi Voice Remote";

    // Save bound MAC to NVS
    s_bound_mac = s_connected_mac;
    s_ble_prefs.putString("bound_mac", s_bound_mac);
    s_ble_prefs.putString("bound_name", s_connected_name);
    app_log("BLE", "Bound and saved remote: %s (%s)", s_connected_name.c_str(), s_bound_mac.c_str());

    return setup_services_and_handshake();
}

static bool connect_to_mac_internal(const NimBLEAddress& address, const String& dev_name) {
    s_ble_state = BLE_STATE_CONNECTING;
    NimBLEDevice::getScan()->stop();

    if (s_client == nullptr) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(new ClientCallbacks(), false);
    } else if (s_client->isConnected()) {
        s_client->disconnect();
    }

    app_log("BLE", "Connecting directly to MAC: %s...", address.toString().c_str());
    if (!s_client->connect(address)) {
        app_log("BLE", "Direct connection to %s failed", address.toString().c_str());
        s_ble_state = BLE_STATE_DISCONNECTED;
        return false;
    }

    s_connected_mac = address.toString().c_str();
    s_connected_name = dev_name.length() > 0 ? dev_name : "Xiaomi Voice Remote";
    s_bound_mac = s_connected_mac;
    s_ble_prefs.putString("bound_mac", s_bound_mac);
    s_ble_prefs.putString("bound_name", s_connected_name);
    app_log("BLE", "Manually paired and saved: %s (%s)", s_connected_name.c_str(), s_bound_mac.c_str());

    return setup_services_and_handshake();
}

extern "C" {

void ble_remote_init(void) {
    s_ble_prefs.begin("ble_conf", false);
    s_bound_mac = s_ble_prefs.getString("bound_mac", "");
    String bound_name = s_ble_prefs.getString("bound_name", "");

    if (s_bound_mac.length() > 0) {
        app_log("BLE", "Loaded previously bound remote: %s (%s)", bound_name.c_str(), s_bound_mac.c_str());
    }

    NimBLEDevice::init("ESP32-RemoteBridge");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    start_scan();
}

void ble_remote_task(void) {
    uint32_t now = millis();

    // 1. Connection watchdog / Auto Re-scan
    if (s_ble_state == BLE_STATE_DISCONNECTED) {
        if (now - s_last_scan_ms > 4000) {
            start_scan();
        }
    }

    // 2. Audio silence watchdog (if voice key released packet dropped)
    if (s_ble_state == BLE_STATE_TALKING) {
        if (now - s_last_audio_ms > BLE_SILENCE_WATCHDOG_MS) {
            app_log("ATVV", "Silence watchdog expired -> force stopping speech");
            s_ble_state = BLE_STATE_CONNECTED;
            key_action_t act = { ACTION_VOICE_RELEASE, DEFAULT_VOICE_MODIFIER, DEFAULT_VOICE_KEY, 0 };
            usb_hid_dispatch_action(&act);
        } else if (now - s_last_extend_ms > BLE_KEEP_ALIVE_INTERVAL) {
            // Keep alive extend active session
            if (s_char_cmd != nullptr) {
                uint8_t cmd_extend[] = { 0x0E, s_session_id };
                s_char_cmd->writeValue(cmd_extend, sizeof(cmd_extend), false);
                s_last_extend_ms = now;
            }
        }
    }
}

ble_remote_state_t ble_remote_get_state(void) {
    return s_ble_state;
}

void ble_remote_trigger_reconnect(void) {
    if (s_client && s_client->isConnected()) {
        s_client->disconnect();
    }
    start_scan();
}

String ble_remote_scan_devices_json(void) {
    app_log("BLE", "Performing full 4s BLE scan for nearby devices...");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->stop();
    NimBLEScanResults results = pScan->start(4, false);

    JsonDocument doc;
    JsonArray arr = doc["devices"].to<JsonArray>();

    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice dev = results.getDevice(i);
        JsonObject obj = arr.add<JsonObject>();
        String name = dev.getName().c_str();
        if (name.length() == 0) name = "Unnamed BLE Device";
        obj["name"] = name;
        obj["mac"] = dev.getAddress().toString().c_str();
        obj["rssi"] = dev.getRSSI();
    }

    String out;
    serializeJson(doc, out);
    return out;
}

bool ble_remote_connect_mac(const String& mac_str) {
    if (mac_str.length() == 0) return false;
    NimBLEAddress addr(mac_str.c_str());
    return connect_to_mac_internal(addr, "Xiaomi Remote");
}

void ble_remote_unpair(void) {
    s_bound_mac = "";
    s_connected_name = "";
    s_connected_mac = "";
    s_ble_prefs.remove("bound_mac");
    s_ble_prefs.remove("bound_name");
    app_log("BLE", "Unpaired and cleared saved remote MAC");
    ble_remote_trigger_reconnect();
}

String ble_remote_get_connected_info(void) {
    JsonDocument doc;
    doc["connected"] = (s_ble_state >= BLE_STATE_CONNECTED);
    doc["name"] = s_connected_name;
    doc["mac"] = s_connected_mac;
    doc["bound_mac"] = s_bound_mac;
    String out;
    serializeJson(doc, out);
    return out;
}

} // extern "C"
