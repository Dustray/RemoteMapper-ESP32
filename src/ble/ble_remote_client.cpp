#include "ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "led_indicator.h"
#include "keymap/key_state_machine.h"
#include "usb/usb_composite.h"
#include "log/app_log.h"
#include "wifi/wifi_manager.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "nimble/nimble/host/include/host/ble_hs.h"
#include "esp_coexist.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>

static ble_remote_state_t              s_ble_state = BLE_STATE_DISCONNECTED;
static esp_coex_prefer_t               s_coex_prefer = ESP_COEX_PREFER_BALANCE;
static NimBLEClient*                   s_client = nullptr;
static NimBLERemoteCharacteristic*     s_char_cmd = nullptr;
static NimBLERemoteCharacteristic*     s_char_aud = nullptr;
static NimBLERemoteCharacteristic*     s_char_ctl = nullptr;
static NimBLERemoteCharacteristic*     s_char_batt = nullptr;
static NimBLEAdvertisedDeviceCallbacks* s_adv_callbacks = nullptr;

static Preferences                     s_ble_prefs;
static String                          s_bound_mac = "";
static uint8_t                         s_bound_addr_type = BLE_ADDR_RANDOM;
static String                          s_connected_name = "";
static String                          s_connected_mac = "";

// 遥控器电量（0-100，-1 = 未知）；连接后首次读取，之后靠通知 + 周期重读更新
static int8_t                          s_battery_level = -1;
static uint32_t                        s_last_batt_read_ms = 0;

// 僵尸链路心跳探测：ATT 读必须对端应用层应答，是唯一可靠的存活探针
// （5s ATVV ping 为 write-no-response，链路层 ACK 即成功，探测不出应用层死亡）。
// 用原生异步 ble_gattc_read + 任务循环超时判定，避免同步 readValue 在死链路上永久阻塞。
#define BATT_PROBE_INTERVAL_MS   60000  // 心跳周期
#define BATT_PROBE_TIMEOUT_MS    10000  // 单次探测无回调即视为超时
#define BATT_PROBE_MAX_FAILS     2      // 连续超时 N 次 -> 强制断开重连
static uint8_t                         s_batt_fail_count = 0;
static bool                            s_batt_probe_pending = false;
static uint32_t                        s_batt_probe_start_ms = 0;

// Asynchronous Connect Request state
static bool                            s_do_connect = false;
static bool                            s_manual_connect_requested = false;
static bool                            s_reconnect_requested = false;
static NimBLEAdvertisedDevice*         s_pending_adv_device = nullptr;
static String                          s_pending_mac = "";
static uint8_t                         s_pending_addr_type = BLE_ADDR_RANDOM;

static uint8_t                         s_session_id = 0;
static uint32_t                        s_last_audio_ms = 0;
static uint32_t                        s_last_extend_ms = 0;
static uint32_t                        s_last_scan_ms = 0;
static uint32_t                        s_last_keepalive_ms = 0;
static size_t                          s_frame_size = AUDIO_DEFAULT_FRAME_BYTES;

// Foreground scan state (Core 1 sets, Core 0 consumes)
static TaskHandle_t                    s_ble_task_handle = nullptr;
static volatile bool                   s_scan_in_progress = false;
static portMUX_TYPE                    s_scan_in_progress_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t                        s_scan_start_ms = 0;
static char                            s_scan_results_buf[8192] = {0};
static portMUX_TYPE                    s_scan_spinlock = portMUX_INITIALIZER_UNLOCKED;

static bool is_foreground_scan_in_progress(void) {
    bool v = false;
    portENTER_CRITICAL(&s_scan_in_progress_mux);
    v = s_scan_in_progress;
    portEXIT_CRITICAL(&s_scan_in_progress_mux);
    return v;
}

static void set_foreground_scan_in_progress(bool v) {
    portENTER_CRITICAL(&s_scan_in_progress_mux);
    s_scan_in_progress = v;
    portEXIT_CRITICAL(&s_scan_in_progress_mux);
}

extern key_mapper_engine_t g_key_engine;

// Forward Declarations
static void start_scan();
static bool do_connect_adv_device(NimBLEAdvertisedDevice* advDevice);
static bool do_connect_mac(const String& mac_str, uint8_t addr_type);
static bool setup_services_and_handshake();

#ifdef __cplusplus
extern "C" {
#endif
static void append_live_scan_device(const String& name, const String& mac, int rssi, int type);
#ifdef __cplusplus
}
#endif

// Audio Notification Callback (ATVV Char 0x03)
static void on_audio_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length == 0) return;
    s_last_audio_ms = millis();
    audio_pipeline_feed_adpcm(&g_audio_pipeline, pData, length);
}

// Battery Level Notification Callback (0x2A19)
static void on_battery_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length >= 1) {
        s_battery_level = (int8_t)pData[0];
        app_log("BLE", "Battery level: %d%%", (int)s_battery_level);
    }
}

// 电池心跳异步读回调：只要收到任何回调（成功或 ATT 错误应答）都证明对端应用层存活。
// 超时无回调（僵尸链路）则不会触发本函数，由 ble_remote_task 的超时判定处理。
static int on_batt_probe_cb(uint16_t conn_handle, const struct ble_gatt_error* error,
                            struct ble_gatt_attr* attr, void* arg) {
    (void)conn_handle;
    (void)arg;
    s_batt_probe_pending = false;
    if (error != nullptr && error->status == 0 && attr != nullptr &&
        attr->om != nullptr && OS_MBUF_PKTLEN(attr->om) >= 1) {
        uint8_t lvl = 0;
        os_mbuf_copydata(attr->om, 0, 1, &lvl);
        s_batt_fail_count = 0;
        if ((int8_t)lvl != s_battery_level) {
            s_battery_level = (int8_t)lvl;
            app_log("BLE", "Battery level: %d%%", (int)s_battery_level);
        }
    } else if (error != nullptr && error->status != 0) {
        // 对端返回 ATT 错误应答 = 应用层存活（能应答），不算链路故障
        s_batt_fail_count = 0;
        app_log("BLE", "Battery probe ATT error rc=%d (app layer alive)", (int)error->status);
    }
    return 0;
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

        key_engine_feed_key(&g_key_engine, MI_KEY_VOICE, true, millis());
        app_log("ATVV", ">>> Voice button PRESSED (session %d)", s_session_id);
    }
    // AUDIO_STOP / MIC_CLOSED / release op (0x00 or 0x08):
    else if (op == 0x00 || op == 0x08) {
        s_ble_state = BLE_STATE_CONNECTED;
        key_engine_feed_key(&g_key_engine, MI_KEY_VOICE, false, millis());
        app_log("ATVV", "<<< Voice button RELEASED (op=0x%02X)", op);

        // Re-arm remote HTT standby
        if (s_char_cmd != nullptr) {
            uint8_t cmd_open[] = { 0x0C, 0x00 };
            s_char_cmd->writeValue(cmd_open, sizeof(cmd_open), false);
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

static uint8_t s_last_hogp_key = 0;

// HOGP HID Report Notification Callback
static void on_hogp_report_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 1) return;

    // 1. Audio / Voice Packets (len > 8, e.g. 20, 120, 240 bytes ADPCM)
    if (length > 8) {
        app_log("HOGP_AUD", "Voice frame len=%d from Char %s", (int)length, pChar->getUUID().toString().c_str());
        s_last_audio_ms = millis();
        audio_pipeline_feed_adpcm(&g_audio_pipeline, pData, length);
        return;
    }

    // Dump raw bytes for diagnostics (only for genuine key reports len <= 8)
    String hex_str = "";
    for (size_t i = 0; i < length; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X ", pData[i]);
        hex_str += buf;
    }
    app_log("HOGP_RAW", "Report (len %d): %s", (int)length, hex_str.c_str());

    // 2. Standard Keyboard & Consumer Reports (len <= 8)
    uint8_t raw_key = 0;
    bool is_pressed = false;

    if (length == 8) {
        // Standard 8-byte Keyboard Report: [modifiers, reserved, key0..key5]
        if (pData[2] != 0) {
            raw_key = pData[2];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    } else if (length == 2) {
        if (pData[1] != 0) {
            raw_key = pData[1];
            is_pressed = true;
        } else if (pData[0] != 0) {
            raw_key = pData[0];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    } else if (length == 1) {
        if (pData[0] != 0) {
            raw_key = pData[0];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    } else if (length >= 3 && length <= 7) {
        if (pData[2] != 0) {
            raw_key = pData[2];
            is_pressed = true;
        } else if (pData[0] != 0) {
            raw_key = pData[0];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    }

    // Auto-release previous key if a new key is pressed without an explicit all-zero release report
    if (s_last_hogp_key != 0 && is_pressed && raw_key != s_last_hogp_key) {
        app_log("HOGP", "Auto-Release key 0x%02X due to new key 0x%02X", s_last_hogp_key, raw_key);
        key_engine_feed_key(&g_key_engine, s_last_hogp_key, false, millis());
        s_last_hogp_key = 0;
    }

    if (is_pressed) {
        s_last_hogp_key = raw_key;
    } else {
        s_last_hogp_key = 0;
    }

    if (raw_key != 0) {
        app_log("HOGP", "Key event: 0x%02X (%s)", raw_key, is_pressed ? "DOWN" : "UP");
        if (raw_key == MI_KEY_VOICE || raw_key == MI_KEY_VOICE_ALT) {
            app_log("VOICE", "Voice button event: 0x%02X (%s)", raw_key, is_pressed ? "DOWN" : "UP");
            if (s_char_cmd != nullptr && is_pressed) {
                uint8_t cmd_open[] = { 0x0C, 0x00 };
                s_char_cmd->writeValue(cmd_open, sizeof(cmd_open), false);
                app_log("ATVV", "Triggered MIC_OPEN on Voice key press");
            }
        }
        key_engine_feed_key(&g_key_engine, raw_key, is_pressed, millis());
    }
}

static bool is_target_remote(NimBLEAdvertisedDevice* dev) {
    String name = dev->getName().c_str();
    String addr = dev->getAddress().toString().c_str();
    addr.toLowerCase();

    // 1. Exact match with previously bound remote (case-insensitive)
    if (s_bound_mac.length() > 0) {
        String bound = s_bound_mac;
        bound.toLowerCase();
        if (addr.equals(bound)) {
            return true;
        }
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
        int rssi = advertisedDevice->getRSSI();
        int type = (int)advertisedDevice->getAddress().getType();

        // 无条件打印：任何广播包都上报，供调试页面实时反馈按键唤醒
        app_log("BLE_ADV", "addr=%s rssi=%d type=%d name=%s",
                addr.c_str(), rssi, type, name.c_str());

        if (is_foreground_scan_in_progress()) {
            String display_name = name.length() > 0 ? name : String("Unnamed BLE Device");
            app_log("BLE_SCAN", "Live: %s (%s, RSSI: %d, Type: %d)",
                    display_name.c_str(), addr.c_str(), rssi, type);
            append_live_scan_device(name, addr, rssi, type);
            return;
        }

        if (name.length() > 0) {
            app_log("BLE_SCAN", "Device: %s (%s, RSSI: %d, Type: %d)",
                    name.c_str(), addr.c_str(), rssi, type);
        }

        if (s_ble_state <= BLE_STATE_SCANNING && is_target_remote(advertisedDevice) && !s_do_connect && !s_manual_connect_requested) {
            app_log("BLE", "Matching Target Remote: %s (%s), queueing connection...", name.c_str(), addr.c_str());
            NimBLEDevice::getScan()->stop();
            if (s_pending_adv_device) delete s_pending_adv_device;
            s_pending_adv_device = new NimBLEAdvertisedDevice(*advertisedDevice);
            s_do_connect = true;
        }
    }
};

// Client Connection Callbacks
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        app_log("BLE", "Remote GATT Connected!");
        s_ble_state = BLE_STATE_CONNECTING;
        pClient->secureConnection();
    }

    void onDisconnect(NimBLEClient* pClient) override {
        app_log("BLE", "Remote Disconnected");
        s_ble_state = BLE_STATE_DISCONNECTED;
        s_do_connect = false;
        s_char_cmd = nullptr;
        s_char_aud = nullptr;
        s_char_ctl = nullptr;
        s_char_batt = nullptr;
        s_batt_probe_pending = false;   // 旧链路的探测作废
        s_batt_fail_count = 0;
        s_last_hogp_key = 0;
        key_engine_release_all(&g_key_engine, millis());
        usb_hid_keyboard_release();
        usb_hid_consumer_release();
        audio_pipeline_stop_session(&g_audio_pipeline);
        led_indicator_set(LED_STATE_WAIT_CONNECTION);
        start_scan();
    }

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) override {
        return true; // Accept remote requested conn params
    }

    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
        if (desc->sec_state.encrypted) {
            app_log("BLE_SEC", "Link Encrypted & Bonded! (Bonded:%d)", desc->sec_state.bonded);
        } else {
            app_log("BLE_SEC", "Encryption not established");
        }
    }
};

static void start_scan() {
    if (s_do_connect || s_ble_state == BLE_STATE_CONNECTING || s_ble_state >= BLE_STATE_CONNECTED) {
        return;
    }
    // 扫描已在跑则只纠正状态，避免重复发起（幂等）
    if (NimBLEDevice::getScan()->isScanning()) {
        s_ble_state = BLE_STATE_SCANNING;
        return;
    }
    // 清理残留的连接流程：connect 超时后底层流程可能未被取消，
    // 导致 ble_gap_disc 返回 EBUSY、扫描永远启动不了（每 10ms 重试刷屏的元凶）
    if (ble_gap_conn_active()) {
        int crc = ble_gap_conn_cancel();
        app_log("BLE", "Cancelled stale connect procedure (rc=%d)", crc);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    s_ble_state = BLE_STATE_SCANNING;
    led_indicator_set(LED_STATE_WAIT_CONNECTION);
    s_last_scan_ms = millis();
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(BLE_SCAN_INTERVAL_MS);
    pScan->setWindow(BLE_SCAN_WINDOW_MS);
    // 三参非阻塞重载：两参版本会阻塞等待首个广播包（持续扫描=永久等待），
    // 在射频安静时会把 BLE 任务卡死
    bool ok = pScan->start(0, (void (*)(NimBLEScanResults))nullptr, false); // 0 = continuous scan until stopped
    if (!ok) {
        // 启动失败：回退状态让重扫分支稍后重试（限流日志，防止 10ms 刷屏）
        s_ble_state = BLE_STATE_DISCONNECTED;
        static uint32_t s_last_scan_fail_log_ms = 0;
        if (millis() - s_last_scan_fail_log_ms > 10000) {
            s_last_scan_fail_log_ms = millis();
            app_log("BLE", "Scan start FAILED - will retry (possible stale connection procedure)");
        }
        return;
    }
    app_log("BLE", "Continuous scanning for Xiaomi Bluetooth Remote active...");
}

static bool setup_services_and_handshake() {
    if (!s_client || !s_client->isConnected()) return false;

    // 1. Security & Bonding
    // HID 报告特征的 CCCD 写入通常要求加密链路；加密失败 = 永远收不到按键报文
    if (!s_client->secureConnection()) {
        app_log("BLE_SEC", "secureConnection FAILED - HID notifications will not flow!");
    } else {
        app_log("BLE_SEC", "secureConnection OK");
    }

    // 2. Fast connection parameters (15ms interval)
    s_client->setDataLen(251);
    s_client->updateConnParams(12, 12, 0, 400);

    // 3. Discover services
    std::vector<NimBLERemoteService*>* pServices = s_client->getServices(true);
    if (!pServices) {
        app_log("BLE", "No GATT services found");
        return false;
    }

    app_log("BLE", "Discovered %d GATT Service(s)", (int)pServices->size());

    // ============ 阶段 1：纯发现（绝不穿插阻塞 GATT IO） ============
    // NimBLE 1.4.x 中 GATT 客户端过程串行执行：发现循环中间插入 read/subscribe
    // 等阻塞 IO 会污染过程流水线，导致后续服务的特征发现瞬间失败且静默返回空
    // （0x1812 HID 特征全空的根因）。feat:battery 提交把电池读+订阅插进了循环。
    // 所有匹配的指针先收集，阶段 2 统一做 IO。
    std::vector<NimBLERemoteCharacteristic*> hogp_reports;   // 0x2A4D / 1812 通知特征
    std::vector<NimBLERemoteCharacteristic*> notify_targets; // ab5e0003/0004 通知特征
    NimBLERemoteCharacteristic* proto_mode = nullptr;        // 0x2A4E
    NimBLERemoteCharacteristic* hid_ctrl = nullptr;          // 0x2A4C

    for (auto* pSvc : *pServices) {
        String svc_uuid = pSvc->getUUID().toString().c_str();
        svc_uuid.toLowerCase();
        app_log("GATT_SVC", "Service: %s", svc_uuid.c_str());

        // Skip known standard BLE metadata services (0x1800 GAP, 0x1801 GATT, 0x180A DIS)
        if (svc_uuid.indexOf("1800") >= 0 || svc_uuid.indexOf("1801") >= 0 ||
            svc_uuid.indexOf("180a") >= 0) {
            continue;
        }

        std::vector<NimBLERemoteCharacteristic*>* pChars = pSvc->getCharacteristics(true);
        if (!pChars || pChars->empty()) {
            // 发现失败在 1.4.x 里是静默的（NIMBLE_LOGE 走 UART0 不可见），必须显式暴露
            app_log("GATT", "WARNING: no characteristics discovered for %s!", svc_uuid.c_str());
            // HID 服务重试一次（瞬时过程冲突可自愈）
            if (svc_uuid.indexOf("1812") >= 0) {
                vTaskDelay(pdMS_TO_TICKS(60));
                pChars = pSvc->getCharacteristics(true);
                if (pChars && !pChars->empty()) {
                    app_log("GATT", "Retry OK: 0x1812 has %d characteristic(s)", (int)pChars->size());
                } else {
                    app_log("GATT", "Retry FAILED: 0x1812 still empty - HID reports unavailable!");
                }
            }
            if (!pChars || pChars->empty()) continue;
        }

        for (auto* pChar : *pChars) {
            String char_uuid = pChar->getUUID().toString().c_str();
            char_uuid.toLowerCase();
            bool can_notif = pChar->canNotify();
            bool can_ind = pChar->canIndicate();
            bool can_wr = pChar->canWrite() || pChar->canWriteNoResponse();
            app_log("GATT_CHAR", "  Char: %s (N:%d, I:%d, W:%d)", char_uuid.c_str(), can_notif ? 1 : 0, can_ind ? 1 : 0, can_wr ? 1 : 0);

            // Match ATVV CMD (ab5e0002)
            if (char_uuid.indexOf("ab5e0002") >= 0) {
                s_char_cmd = pChar;
                app_log("ATVV", "Matched ATVV CMD Char: %s", char_uuid.c_str());
            }
            // Match ATVV AUD (ab5e0003)
            else if (char_uuid.indexOf("ab5e0003") >= 0) {
                s_char_aud = pChar;
                if (can_notif) notify_targets.push_back(pChar);
            }
            // Match ATVV CTL (ab5e0004)
            else if (char_uuid.indexOf("ab5e0004") >= 0) {
                s_char_ctl = pChar;
                if (can_notif) notify_targets.push_back(pChar);
            }
            // Match Protocol Mode (0x2A4E)
            else if (char_uuid.indexOf("2a4e") >= 0) {
                if (can_wr) proto_mode = pChar;
            }
            // Match HID Control Point (0x2A4C)
            else if (char_uuid.indexOf("2a4c") >= 0) {
                if (can_wr) hid_ctrl = pChar;
            }
            // Match HOGP Report (0x2A4D) or any notify char in 0x1812 service
            else if (char_uuid.indexOf("2a4d") >= 0 || svc_uuid.indexOf("1812") >= 0) {
                if (can_notif || can_ind) hogp_reports.push_back(pChar);
            }
            // Match Battery Level (0x2A19)
            else if (char_uuid.indexOf("2a19") >= 0) {
                s_char_batt = pChar;
            }
        }
    }

    app_log("BLE", "Discovery done: %d report char(s), cmd=%s, aud=%s, ctl=%s, batt=%s",
            (int)hogp_reports.size(),
            s_char_cmd ? "Y" : "N", s_char_aud ? "Y" : "N",
            s_char_ctl ? "Y" : "N", s_char_batt ? "Y" : "N");

    if (hogp_reports.empty()) {
        app_log("HOGP", "No report characteristic found - button presses will NOT work!");
    }

    // ============ 阶段 2：统一执行阻塞 IO（读 / 写 / 订阅） ============
    int sub_count = 0;

    // HID Control Point -> Exit Suspend
    if (hid_ctrl) {
        uint8_t cp = 0x00;
        hid_ctrl->writeValue(&cp, 1, false);
    }
    // Protocol Mode -> Report Mode (0x01)
    if (proto_mode) {
        uint8_t mode = 0x01;
        proto_mode->writeValue(&mode, 1, false);
        app_log("HOGP", "Set Protocol Mode to Report Mode (0x01)");
    }
    // HOGP Report CCCD 订阅（HID 报文的入口，失败则按键无反应）
    for (auto* pChar : hogp_reports) {
        bool sub_ok = pChar->subscribe(true, on_hogp_report_notify, false);
        sub_count++;
        app_log("HOGP", "Subscribe Report Char %s: %s",
                pChar->getUUID().toString().c_str(), sub_ok ? "ok" : "FAILED (CCCD rejected)");
    }
    // ATVV AUD/CTL CCCD 订阅
    for (auto* pChar : notify_targets) {
        if (pChar == s_char_aud) {
            pChar->subscribe(true, on_audio_notify, false);
            app_log("ATVV", "Subscribed to ATVV AUD Char");
        } else {
            pChar->subscribe(true, on_ctl_notify, false);
            app_log("ATVV", "Subscribed to ATVV CTL Char");
        }
        sub_count++;
    }
    // 电池：初始读 + 订阅 + 心跳状态复位
    if (s_char_batt) {
        NimBLEAttValue val = s_char_batt->readValue();
        if (val.length() >= 1) {
            s_battery_level = (int8_t)val[0];
            app_log("BLE", "Battery level: %d%%", (int)s_battery_level);
        }
        bool sub_ok = s_char_batt->subscribe(true, on_battery_notify, false);
        sub_count++;
        app_log("BLE", "Battery Level subscribe: %s", sub_ok ? "ok" : "FAILED");
        s_last_batt_read_ms = millis();
        s_batt_fail_count = 0;      // 新链路，重置僵尸探测计数
        s_batt_probe_pending = false;
    }

    app_log("BLE", "Total Subscribed Characteristic(s): %d", sub_count);

    // 4. ATVV Handshake
    if (s_char_cmd) {
        uint8_t cmd_caps[] = { 0x0A, 0x01, 0x00, 0x00, 0x03, 0x03 };
        s_char_cmd->writeValue(cmd_caps, sizeof(cmd_caps), false);
        vTaskDelay(pdMS_TO_TICKS(20));
        uint8_t cmd_open[] = { 0x0C, 0x00 };
        s_char_cmd->writeValue(cmd_open, sizeof(cmd_open), false);
        app_log("ATVV", "Handshake completed (MIC_OPEN active)");
    }

    s_ble_state = BLE_STATE_CONNECTED;
    led_indicator_set(LED_STATE_CONNECTED);
    s_last_keepalive_ms = millis();
    return true;
}

static bool do_connect_adv_device(NimBLEAdvertisedDevice* advDevice) {
    if (!advDevice) return false;
    s_ble_state = BLE_STATE_CONNECTING;

    if (s_client == nullptr) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(new ClientCallbacks(), false);
    } else if (s_client->isConnected()) {
        s_client->disconnect();
    }

    app_log("BLE", "Connecting to Advertised Device: %s (%s, Type: %d)...", 
            advDevice->getName().c_str(), advDevice->getAddress().toString().c_str(), 
            (int)advDevice->getAddress().getType());

    if (!s_client->connect(advDevice)) {
        app_log("BLE", "Connection Failed to %s", advDevice->getAddress().toString().c_str());
        s_ble_state = BLE_STATE_DISCONNECTED;
        start_scan();
        return false;
    }

    s_connected_name = advDevice->getName().c_str();
    s_connected_mac = advDevice->getAddress().toString().c_str();
    s_bound_addr_type = advDevice->getAddress().getType();
    if (s_connected_name.length() == 0) s_connected_name = "Xiaomi Voice Remote";

    // Save bound MAC to NVS
    s_bound_mac = s_connected_mac;
    s_ble_prefs.putString("bound_mac", s_bound_mac);
    s_ble_prefs.putString("bound_name", s_connected_name);
    s_ble_prefs.putUChar("bound_type", s_bound_addr_type);
    app_log("BLE", "Bound and saved remote: %s (%s, Type: %d)", s_connected_name.c_str(), s_bound_mac.c_str(), (int)s_bound_addr_type);

    return setup_services_and_handshake();
}

static bool do_connect_mac(const String& mac_str, uint8_t addr_type) {
    s_ble_state = BLE_STATE_CONNECTING;
    NimBLEDevice::getScan()->stop();

    if (s_client == nullptr) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(new ClientCallbacks(), false);
    } else if (s_client->isConnected()) {
        s_client->disconnect();
    }

    // Try primary addr_type
    NimBLEAddress addr1(mac_str.c_str(), addr_type);
    app_log("BLE", "Connecting to MAC: %s (Type: %d)...", mac_str.c_str(), (int)addr_type);
    bool ok = s_client->connect(addr1);

    // If failed, try alternative addr_type (Public vs Random)
    if (!ok) {
        uint8_t alt_type = (addr_type == BLE_ADDR_RANDOM) ? BLE_ADDR_PUBLIC : BLE_ADDR_RANDOM;
        NimBLEAddress addr2(mac_str.c_str(), alt_type);
        app_log("BLE", "Retrying with alternate Type: %d...", (int)alt_type);
        ok = s_client->connect(addr2);
        if (ok) addr_type = alt_type;
    }

    if (!ok) {
        app_log("BLE", "Direct connection to %s failed on both address types", mac_str.c_str());
        s_ble_state = BLE_STATE_DISCONNECTED;
        start_scan();
        return false;
    }

    s_connected_mac = mac_str;
    s_connected_name = "Xiaomi Voice Remote";
    s_bound_mac = s_connected_mac;
    s_bound_addr_type = addr_type;
    s_ble_prefs.putString("bound_mac", s_bound_mac);
    s_ble_prefs.putString("bound_name", s_connected_name);
    s_ble_prefs.putUChar("bound_type", s_bound_addr_type);
    app_log("BLE", "Manually paired and saved: %s (%s, Type: %d)", s_connected_name.c_str(), s_bound_mac.c_str(), (int)s_bound_addr_type);

    return setup_services_and_handshake();
}

extern "C" {

void ble_remote_init(void) {
    s_ble_prefs.begin("ble_conf", false);
    s_bound_mac = s_ble_prefs.getString("bound_mac", "");
    String bound_name = s_ble_prefs.getString("bound_name", "");
    s_bound_addr_type = s_ble_prefs.getUChar("bound_type", BLE_ADDR_RANDOM);

    if (s_bound_mac.length() > 0) {
        app_log("BLE", "Loaded previously bound remote: %s (%s, Type: %d)", bound_name.c_str(), s_bound_mac.c_str(), (int)s_bound_addr_type);
    }

    if (s_ble_task_handle == nullptr) {
        s_ble_task_handle = xTaskGetCurrentTaskHandle();
    }

    NimBLEDevice::init("ESP32-RemoteBridge");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (s_adv_callbacks == nullptr) {
        s_adv_callbacks = new AdvertisedDeviceCallbacks();
    }
    pScan->setAdvertisedDeviceCallbacks(s_adv_callbacks);

    // Start continuous fast scan immediately (instant catch when remote advertises)
    start_scan();
}

static void build_and_store_scan_results(void) {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    NimBLEScanResults results = pScan->getResults();

    JsonDocument doc;
    doc["type"] = "ble";
    doc["status"] = "ok";
    doc["scanning"] = false;
    JsonArray arr = doc["devices"].to<JsonArray>();

    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice dev = results.getDevice(i);
        JsonObject obj = arr.add<JsonObject>();
        String name = dev.getName().c_str();
        if (name.length() == 0) name = "Unnamed BLE Device";
        obj["name"] = name;
        obj["mac"] = dev.getAddress().toString().c_str();
        obj["rssi"] = dev.getRSSI();
        obj["type"] = (int)dev.getAddress().getType();
    }

    String out;
    size_t serialized_len = serializeJson(doc, out);
    if (serialized_len >= sizeof(s_scan_results_buf)) {
        app_log("BLE", "Scan result too large (%d bytes), truncating device list", (int)serialized_len);
        while (arr.size() > 0 && out.length() >= sizeof(s_scan_results_buf) - 4) {
            arr.remove(arr.size() - 1);
            out.clear();
            serializeJson(doc, out);
        }
        if (out.length() >= sizeof(s_scan_results_buf) - 4) {
            doc.clear();
            doc["status"] = "ok";
            doc["scanning"] = false;
            doc["devices"] = JsonArray();
            out.clear();
            serializeJson(doc, out);
        }
    }

    pScan->clearResults();

    // Do not immediately restart continuous scan after a foreground scan;
    // the caller has the full result and will restart scanning if needed.

    portENTER_CRITICAL(&s_scan_spinlock);
    size_t len = out.length();
    if (len >= sizeof(s_scan_results_buf)) len = sizeof(s_scan_results_buf) - 1;
    memcpy(s_scan_results_buf, out.c_str(), len);
    s_scan_results_buf[len] = '\0';
    portEXIT_CRITICAL(&s_scan_spinlock);

    set_foreground_scan_in_progress(false);
    s_scan_start_ms = 0;
    app_log("BLE", "Scan done: %d devices (json %d bytes)", results.getCount(), (int)len);
}

static void append_live_scan_device(const String& name, const String& mac, int rssi, int type) {
    portENTER_CRITICAL(&s_scan_spinlock);
    JsonDocument doc;
    if (s_scan_results_buf[0] != '\0') {
        deserializeJson(doc, s_scan_results_buf);
    }
    doc["type"] = "ble";
    doc["status"] = "ok";
    doc["scanning"] = true;
    JsonArray arr = doc["devices"].to<JsonArray>();

    // Deduplicate by MAC
    for (JsonObject existing : arr) {
        const char* existing_mac = existing["mac"];
        if (existing_mac && mac.equals(existing_mac)) {
            existing["rssi"] = rssi;
            String out;
            serializeJson(doc, out);
            size_t len = out.length();
            if (len >= sizeof(s_scan_results_buf)) len = sizeof(s_scan_results_buf) - 1;
            memcpy(s_scan_results_buf, out.c_str(), len);
            s_scan_results_buf[len] = '\0';
            portEXIT_CRITICAL(&s_scan_spinlock);
            return;
        }
    }

    JsonObject obj = arr.add<JsonObject>();
    obj["name"] = name.length() > 0 ? name : "Unnamed BLE Device";
    obj["mac"] = mac;
    obj["rssi"] = rssi;
    obj["type"] = type;

    String out;
    size_t serialized_len = serializeJson(doc, out);
    if (serialized_len >= sizeof(s_scan_results_buf)) {
        app_log("BLE", "Live scan list full, dropping unnamed devices");
        for (size_t i = 0; i < arr.size(); ) {
            const char* n = arr[i]["name"];
            if (n && strcmp(n, "Unnamed BLE Device") == 0) {
                arr.remove(i);
            } else {
                i++;
            }
        }
        out.clear();
        serializeJson(doc, out);
        if (out.length() >= sizeof(s_scan_results_buf)) {
            portEXIT_CRITICAL(&s_scan_spinlock);
            return;
        }
    }

    size_t len = out.length();
    if (len >= sizeof(s_scan_results_buf)) len = sizeof(s_scan_results_buf) - 1;
    memcpy(s_scan_results_buf, out.c_str(), len);
    s_scan_results_buf[len] = '\0';
    portEXIT_CRITICAL(&s_scan_spinlock);
}

static void check_foreground_scan_complete(void) {
    if (!is_foreground_scan_in_progress()) return;

    NimBLEScan* pScan = NimBLEDevice::getScan();
    uint32_t elapsed = millis() - s_scan_start_ms;

    // NimBLE's start(duration) does not always stop reliably when no callback is supplied.
    // Force stop after the requested 8-second window plus a small margin.
    if (elapsed >= 8500 || !pScan->isScanning()) {
        if (pScan->isScanning()) {
            app_log("BLE", "Foreground scan reached 8s, stopping");
            pScan->stop();
            uint32_t t0 = millis();
            while (pScan->isScanning() && (millis() - t0) < 300) delay(1);
        }
        build_and_store_scan_results();
    }
}

static void do_foreground_scan(void) {
    app_log("BLE", "Starting foreground scan");
    NimBLEScan* pScan = NimBLEDevice::getScan();

    // Ensure the advertised-device callback is registered; NimBLE may drop it
    // across stop/start cycles on some versions.
    if (s_adv_callbacks == nullptr) {
        s_adv_callbacks = new AdvertisedDeviceCallbacks();
    }
    pScan->setAdvertisedDeviceCallbacks(s_adv_callbacks);

    if (pScan->isScanning()) {
        app_log("BLE", "Stopping active scan before foreground scan");
        pScan->stop();
        uint32_t t0 = millis();
        while (pScan->isScanning() && (millis() - t0) < 500) delay(1);
    }
    delay(20);
    pScan->clearResults();
    s_scan_results_buf[0] = '\0';
    s_scan_start_ms = millis();

    if (!pScan->start(8, nullptr, false)) {
        app_log("BLE", "Foreground scan start() failed");
        build_and_store_scan_results();
    } else {
        app_log("BLE", "Foreground scan running");
    }
}

// 共存优先级驱动：BLE 未连接（扫描/握手期）抢占射频，规避 Wi-Fi 饿死 BLE 接收；
// 连接后恢复平衡，让音频流与 Wi-Fi 网页共存。状态不变时零开销。
static void update_coex_preference() {
    esp_coex_prefer_t want =
        (s_ble_state < BLE_STATE_CONNECTED) ? ESP_COEX_PREFER_BT
                                            : ESP_COEX_PREFER_BALANCE;
    if (want != s_coex_prefer) {
        esp_coex_preference_set(want);
        s_coex_prefer = want;
        app_log("BLE", "coex preference -> %s (state=%d)",
                want == ESP_COEX_PREFER_BT ? "BT" : "BALANCE", (int)s_ble_state);
    }
}

void ble_remote_task(void) {
    uint32_t now = millis();
    update_coex_preference();

    if (s_ble_task_handle != nullptr &&
        ulTaskNotifyTake(pdTRUE, 0) > 0) {
        app_log("BLE", "Task received foreground scan request (notify)");
        // If a foreground scan is already in progress, just let it continue;
        // do not restart it, otherwise it will never complete.
        if (!is_foreground_scan_in_progress()) {
            set_foreground_scan_in_progress(true);
            do_foreground_scan();
        }
        return;
    }

    check_foreground_scan_complete();

    // 0. 处理外部重连请求（在 BLE 任务上下文串行执行，避免跨任务并发崩溃）
    if (s_reconnect_requested) {
        s_reconnect_requested = false;
        if (s_client && s_client->isConnected()) {
            app_log("BLE", "Reconnect request -> disconnecting current link");
            s_client->disconnect();   // onDisconnect 回调里会 start_scan()
        } else {
            start_scan();
        }
    }

    // 1. Process asynchronous connection requests from FreeRTOS task
    //    Manual MAC connection has priority over auto-scan advertisement matching.
    if (s_do_connect || s_manual_connect_requested) {
        bool was_manual = s_manual_connect_requested;
        s_do_connect = false;
        s_manual_connect_requested = false;

        if (was_manual && s_pending_mac.length() > 0) {
            // Cancel any pending auto-scan device; user explicitly chose a MAC.
            if (s_pending_adv_device) {
                delete s_pending_adv_device;
                s_pending_adv_device = nullptr;
            }
            app_log("BLE", "Manual connect request for MAC: %s", s_pending_mac.c_str());
            do_connect_mac(s_pending_mac, s_pending_addr_type);
            s_pending_mac = "";
        } else if (s_pending_adv_device) {
            do_connect_adv_device(s_pending_adv_device);
            delete s_pending_adv_device;
            s_pending_adv_device = nullptr;
        } else if (s_pending_mac.length() > 0) {
            do_connect_mac(s_pending_mac, s_pending_addr_type);
            s_pending_mac = "";
        }
    }

    // 2. Auto Re-scan: If not connected, not connecting, and scan is inactive, restart continuous scan
    if (s_ble_state < BLE_STATE_CONNECTING && !s_do_connect) {
        if (!NimBLEDevice::getScan()->isScanning()) {
            start_scan();
        }
    }

    // 3. Audio silence watchdog (if voice key released packet dropped over BLE)
    if (s_ble_state == BLE_STATE_TALKING) {
        if (now - s_last_audio_ms > BLE_SILENCE_WATCHDOG_MS) {
            app_log("ATVV", "Silence watchdog expired -> force stopping speech");
            s_ble_state = BLE_STATE_CONNECTED;
            key_engine_feed_key(&g_key_engine, MI_KEY_VOICE, false, now);

            // Re-arm remote HTT standby
            if (s_char_cmd != nullptr) {
                uint8_t cmd_open[] = { 0x0C, 0x00 };
                s_char_cmd->writeValue(cmd_open, sizeof(cmd_open), false);
            }
        }
    }

    // 4. Periodic keep-alive for remote connection retention
    if (s_ble_state == BLE_STATE_CONNECTED) {
        if (now - s_last_keepalive_ms > 5000) {
            s_last_keepalive_ms = now;
            if (s_char_cmd != nullptr) {
                uint8_t ping[] = { 0x0C, 0x00 }; // Re-arm HTT standby
                s_char_cmd->writeValue(ping, sizeof(ping), false);
            }
        }

        // 5. Battery heartbeat every 60s（异步 ATT 读 = 僵尸链路探针）。
        //    5s ATVV ping 是 write-no-response 探测不出应用层死亡；无应答的读可以。
        //    连续 2 次超时 -> 强制断开重扫重连，实现僵尸链路自愈。
        if (s_char_batt != nullptr) {
            if (s_batt_probe_pending) {
                if (now - s_batt_probe_start_ms > BATT_PROBE_TIMEOUT_MS) {
                    s_batt_probe_pending = false;
                    s_batt_fail_count++;
                    app_log("BLE", "Battery heartbeat timeout (%d/%d) - zombie link suspected",
                            (int)s_batt_fail_count, (int)BATT_PROBE_MAX_FAILS);
                    if (s_batt_fail_count >= BATT_PROBE_MAX_FAILS) {
                        s_batt_fail_count = 0;
                        app_log("BLE", "Forcing reconnect to recover stale link");
                        ble_remote_trigger_reconnect();
                    }
                }
            } else if (now - s_last_batt_read_ms > BATT_PROBE_INTERVAL_MS) {
                s_last_batt_read_ms = now;
                s_batt_probe_pending = true;
                s_batt_probe_start_ms = now;
                int rc = ble_gattc_read(s_client->getConnId(), s_char_batt->getHandle(),
                                        on_batt_probe_cb, NULL);
                if (rc != 0) {
                    s_batt_probe_pending = false;
                    app_log("BLE", "Battery probe start failed rc=%d", rc);
                }
            }
        }
    }
}

ble_remote_state_t ble_remote_get_state(void) {
    return s_ble_state;
}

int8_t ble_remote_get_battery(void) {
    return s_battery_level;
}

void ble_remote_trigger_reconnect(void) {
    // 只置标志，由 ble_remote_task 在 BLE 任务上下文串行执行。
    // 此函数可能从主任务（CLI/Web）调用：直接 disconnect()/start_scan() 会与
    // host 任务的 onDisconnect 回调并发操作扫描器和 TinyUSB，曾导致固件崩溃。
    s_do_connect = false;
    s_manual_connect_requested = false;
    s_reconnect_requested = true;
}

String ble_remote_scan_devices_json(void) {
    // While a scan is active, return the live accumulated list so UI updates immediately.
    if (is_foreground_scan_in_progress()) {
        portENTER_CRITICAL(&s_scan_spinlock);
        String out(s_scan_results_buf);
        portEXIT_CRITICAL(&s_scan_spinlock);
        if (out.length() == 0) {
            return "{\"type\":\"ble\",\"status\":\"scanning\",\"scanning\":true,\"devices\":[]}";
        }
        return out;
    }

    if (s_scan_results_buf[0] != '\0') {
        String out(s_scan_results_buf);
        s_scan_results_buf[0] = '\0';
        return out;
    }

    if (s_ble_task_handle != nullptr) {
        BaseType_t result = xTaskNotifyGive(s_ble_task_handle);
        if (result != pdPASS) {
            app_log("BLE", "Foreground scan notify failed");
        } else {
            app_log("BLE", "Foreground scan requested from serial UI");
        }
    } else {
        app_log("BLE", "Foreground scan failed: BLE task handle not ready");
    }
    return "{\"type\":\"ble\",\"status\":\"scanning\",\"scanning\":true,\"devices\":[]}";
}

bool ble_remote_connect_mac(const String& mac_str) {
    if (mac_str.length() == 0) return false;

    // Stop active scanning immediately and mark a manual-connect request.
    // This prevents the continuous-scan auto-matching from overriding the
    // user's explicit device selection.
    NimBLEDevice::getScan()->stop();
    if (s_pending_adv_device) {
        delete s_pending_adv_device;
        s_pending_adv_device = nullptr;
    }

    s_pending_mac = mac_str;
    s_pending_addr_type = BLE_ADDR_RANDOM;
    s_manual_connect_requested = true;
    s_do_connect = true;
    app_log("BLE", "Queued manual connection to %s", mac_str.c_str());
    return true;
}

void ble_remote_unpair(void) {
    // 必须删除 NimBLE 持久化的 bond（LTK），否则重连时仍拿旧密钥加密：
    // 加密失败 -> HID CCCD 写被拒 -> 永远收不到按键报文（僵尸症状的元凶之一）
    uint16_t bond_cnt = 0;
    bond_cnt = (uint16_t)NimBLEDevice::getNumBonds();
    if (bond_cnt > 0) {
        app_log("BLE", "Deleting %d stored bond(s)...", (int)bond_cnt);
        NimBLEDevice::deleteAllBonds();
    }
    s_bound_mac = "";
    s_connected_name = "";
    s_connected_mac = "";
    s_ble_prefs.remove("bound_mac");
    s_ble_prefs.remove("bound_name");
    s_ble_prefs.remove("bound_type");
    app_log("BLE", "Unpaired: bonds + saved MAC cleared, re-pairing required");
    ble_remote_trigger_reconnect();
}

String ble_remote_get_connected_info(void) {
    JsonDocument doc;
    doc["connected"] = (s_ble_state >= BLE_STATE_CONNECTED);
    doc["name"] = s_connected_name;
    doc["mac"] = s_connected_mac;
    doc["bound_mac"] = s_bound_mac;
    doc["battery"] = (int)s_battery_level;  // -1 = 未知
    doc["wifi_connected"] = wifi_manager_is_sta_connected();
    doc["wifi_ip"] = wifi_manager_get_sta_ip();
    String out;
    serializeJson(doc, out);
    return out;
}

} // extern "C"
