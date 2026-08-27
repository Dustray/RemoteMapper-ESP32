#include "ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "usb/usb_composite.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

static ble_remote_state_t  s_ble_state = BLE_STATE_DISCONNECTED;
static NimBLEClient*       s_client = nullptr;
static NimBLERemoteChar*   s_char_cmd = nullptr;
static NimBLERemoteChar*   s_char_aud = nullptr;
static NimBLERemoteChar*   s_char_ctl = nullptr;

static uint8_t             s_session_id = 0;
static uint32_t            s_last_audio_ms = 0;
static uint32_t            s_last_extend_ms = 0;
static uint32_t            s_last_scan_ms = 0;
static size_t              s_frame_size = AUDIO_DEFAULT_FRAME_BYTES;

extern key_mapper_engine_t g_key_engine;

// Forward Declarations
static void start_scan();
static bool connect_to_remote(NimBLEAdvertisedDevice* advDevice);

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
        Serial.printf("[ATVV] >>> Voice button PRESSED (session %d)\n", s_session_id);
    }
    // AUDIO_STOP / MIC_CLOSED / release op:
    else if (op == 0x00) {
        if (s_ble_state == BLE_STATE_TALKING) {
            s_ble_state = BLE_STATE_CONNECTED;
            key_action_t act = { ACTION_VOICE_RELEASE, DEFAULT_VOICE_MODIFIER, DEFAULT_VOICE_KEY, 0 };
            usb_hid_dispatch_action(&act);
            Serial.println("[ATVV] <<< Voice button RELEASED");

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
        Serial.printf("[ATVV] CAPS: ver=0x%04X, frame_size=%d\n", ver, s_frame_size);
    }
    // AUDIO_SYNC: op == 0x0A
    else if (op == 0x0A && length >= 7) {
        int16_t pred = (int16_t)((pData[4] << 8) | pData[5]);
        int8_t step_idx = (int8_t)pData[6];
        audio_pipeline_sync(&g_audio_pipeline, pred, step_idx);
        Serial.printf("[ATVV] SYNC: pred=%d, step=%d\n", pred, step_idx);
    }
}

// HOGP HID Report Notification Callback
static void on_hogp_report_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 1) return;
    uint8_t raw_key = pData[0];
    bool is_pressed = (length > 1) ? (pData[1] != 0) : (raw_key != 0);

    Serial.printf("[HOGP] Key byte: 0x%02X (%s)\n", raw_key, is_pressed ? "DOWN" : "UP");
    key_engine_feed_key(&g_key_engine, raw_key, is_pressed, millis());
}

// Advertised Device Scan Callbacks
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        String name = advertisedDevice->getName().c_str();
        String addr = advertisedDevice->getAddress().toString().c_str();
        addr.toLowerCase();

        bool match_name = name.indexOf(BLE_REMOTE_NAME_PREFIX) >= 0;
        bool match_mac = addr.startsWith(BLE_REMOTE_MAC_PREFIX);

        if (match_name || match_mac) {
            Serial.printf("[BLE] Found Target Remote: %s (%s), RSSI: %d\n", name.c_str(), addr.c_str(), advertisedDevice->getRSSI());
            NimBLEDevice::getScan()->stop();
            connect_to_remote(advertisedDevice);
        }
    }
};

// Client Connection Callbacks
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        Serial.println("[BLE] Remote Connected");
        s_ble_state = BLE_STATE_CONNECTED;
    }

    void onDisconnect(NimBLEClient* pClient) override {
        Serial.println("[BLE] Remote Disconnected");
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
    pScan->start(5, false);
    Serial.println("[BLE] Scanning for Xiaomi Remote (MI RC)...");
}

static bool connect_to_remote(NimBLEAdvertisedDevice* advDevice) {
    s_ble_state = BLE_STATE_CONNECTING;
    if (s_client == nullptr) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(new ClientCallbacks(), false);
    }

    if (!s_client->connect(advDevice)) {
        Serial.println("[BLE] Connection Failed");
        s_ble_state = BLE_STATE_DISCONNECTED;
        return false;
    }

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
            Serial.println("[ATVV] Handshake completed successfully");
        }
    }

    // Discover HOGP Service
    NimBLERemoteService* pHogpSvc = s_client->getService(NimBLEUUID((uint16_t)HOGP_SVC_UUID));
    if (pHogpSvc) {
        NimBLERemoteCharacteristic* pReportChar = pHogpSvc->getCharacteristic(NimBLEUUID((uint16_t)HOGP_REPORT_CHAR_UUID));
        if (pReportChar && pReportChar->canNotify()) {
            pReportChar->subscribe(true, on_hogp_report_notify);
            Serial.println("[HOGP] Subscribed to HID Key Reports");
        }
    }

    s_ble_state = BLE_STATE_CONNECTED;
    return true;
}

extern "C" {

void ble_remote_init(void) {
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
            Serial.println("[ATVV] Silence watchdog expired -> force stopping speech");
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

} // extern "C"
