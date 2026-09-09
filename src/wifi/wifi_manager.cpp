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

// ---------- STA 连接退避状态机 ----------
// 路由器不可达时，驱动级自动重连每次都会全信道扫射射频，把 BLE 扫描/接收
// 彻底饿死（共存 PREFER_BT 也挡不住连接期的信道切换风暴）。改为自管退避：
// 单次尝试 20s 内连不上就主动断开停手，按 5s→60s 指数退避再试；
// 静默窗口让 BLE 正常收广播。连接成功后退避间隔复位。
#define STA_ATTEMPT_TIMEOUT_MS  20000
#define STA_BACKOFF_MIN_MS      5000
#define STA_BACKOFF_MAX_MS      60000
static bool     s_sta_attempting = false;        // 一次连接尝试进行中
static bool     s_sta_force_attempt = false;     // 外部要求立即发起一轮尝试
static uint32_t s_sta_attempt_start_ms = 0;
static uint32_t s_sta_next_attempt_ms = 0;
static uint32_t s_sta_backoff_ms = STA_BACKOFF_MIN_MS;

void wifi_manager_init(void) {
    s_prefs.begin("wifi_conf", false);
    String sta_ssid = s_prefs.getString("ssid", "");
    String ap_pass  = s_prefs.getString("ap_pass", "");

    // Set Wi-Fi Mode
    WiFi.mode(WIFI_AP_STA);
    // 关闭驱动级无限自动重连，改由下方退避状态机管理，避免射频风暴饿死 BLE
    WiFi.setAutoReconnect(false);

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

    // 3. Connect to Home Wi-Fi if saved (由退避状态机发起首次尝试)
    if (sta_ssid.length() > 0) {
        s_sta_configured = true;
        app_log("WIFI", "Home Wi-Fi configured: %s (first connect attempt queued)", sta_ssid.c_str());
        s_sta_force_attempt = true;
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

    if (!s_sta_configured) return;

    uint32_t now = millis();
    bool connected = (WiFi.status() == WL_CONNECTED);

    // 连接状态变化：连上时复位退避并记日志；掉线时立即安排重试
    static bool s_logged_connected = false;
    if (connected && !s_logged_connected) {
        s_logged_connected = true;
        s_sta_attempting = false;
        s_sta_backoff_ms = STA_BACKOFF_MIN_MS;
        app_log("WIFI", "Connected to Home Wi-Fi! Local IP: %s", WiFi.localIP().toString().c_str());
    } else if (!connected && s_logged_connected) {
        s_logged_connected = false;
        s_sta_force_attempt = true;
        app_log("WIFI", "STA lost connection (status=%d)", (int)WiFi.status());
    }

    if (connected) return;

    // 尝试阶段：超时（20s）未连上 -> 主动断开停手，进入退避等待
    if (s_sta_attempting) {
        if (now - s_sta_attempt_start_ms > STA_ATTEMPT_TIMEOUT_MS) {
            app_log("WIFI", "STA connect attempt failed (status=%d), back off %us",
                    (int)WiFi.status(), (unsigned)(s_sta_backoff_ms / 1000));
            WiFi.disconnect(false, false);
            s_sta_attempting = false;
            s_sta_next_attempt_ms = now + s_sta_backoff_ms;
            s_sta_backoff_ms = (s_sta_backoff_ms * 2 > STA_BACKOFF_MAX_MS)
                                   ? STA_BACKOFF_MAX_MS : s_sta_backoff_ms * 2;
        }
        return;
    }

    // 退避等待结束（或外部强制）-> 发起一轮新尝试
    if (s_sta_force_attempt || (int32_t)(now - s_sta_next_attempt_ms) >= 0) {
        s_sta_force_attempt = false;
        String sta_ssid = s_prefs.getString("ssid", "");
        String sta_pass = s_prefs.getString("pass", "");
        if (sta_ssid.length() == 0) return;
        app_log("WIFI", "STA connect attempt -> %s", sta_ssid.c_str());
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
        s_sta_attempting = true;
        s_sta_attempt_start_ms = now;
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

static volatile bool s_wifi_scan_requested = false;
static volatile bool s_wifi_scan_in_progress = false;
static String s_wifi_scan_results;

static void start_wifi_scan_async(void) {
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    s_wifi_scan_in_progress = true;
}

String wifi_manager_scan_json(void) {
    if (s_wifi_scan_in_progress) {
        int16_t status = WiFi.scanComplete();
        if (status == WIFI_SCAN_RUNNING) {
            return "{\"type\":\"wifi\",\"status\":\"scanning\",\"scanning\":true}";
        }

        if (status >= 0) {
            JsonDocument doc;
            doc["type"] = "wifi";
            doc["status"] = "ok";
            doc["scanning"] = false;
            JsonArray arr = doc["networks"].to<JsonArray>();
            for (int16_t i = 0; i < status; i++) {
                JsonObject obj = arr.add<JsonObject>();
                obj["ssid"] = WiFi.SSID(i);
                obj["rssi"] = WiFi.RSSI(i);
                obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            }
            serializeJson(doc, s_wifi_scan_results);
            WiFi.scanDelete();
            s_wifi_scan_in_progress = false;
            return s_wifi_scan_results;
        }

        WiFi.scanDelete();
        s_wifi_scan_in_progress = false;
        return "{\"type\":\"wifi\",\"status\":\"ok\",\"scanning\":false,\"networks\":[]}";
    }

    start_wifi_scan_async();
    return "{\"type\":\"wifi\",\"status\":\"scanning\",\"scanning\":true}";
}

bool wifi_manager_save_sta_config(const String& ssid, const String& password) {
    if (ssid.length() == 0) return false;

    s_prefs.putString("ssid", ssid);
    s_prefs.putString("pass", password);
    s_sta_configured = true;

    app_log("WIFI", "Saved new Wi-Fi credentials for: %s, connecting...", ssid.c_str());
    WiFi.disconnect(false, false);
    s_sta_backoff_ms = STA_BACKOFF_MIN_MS;
    s_sta_force_attempt = true;   // 交由退避状态机立即发起连接
    return true;
}

void wifi_manager_suspend(void) {
    // wifi_off 调用：Wi-Fi 驱动已停止，挂起退避重试，避免对已停止的驱动反复 begin
    s_sta_attempting = false;
    s_sta_next_attempt_ms = millis() + 60000;
}

void wifi_manager_request_sta_connect(void) {
    // wifi_on / 外部恢复调用：下一轮 task 循环立即发起连接尝试
    if (!s_sta_configured) return;
    s_sta_force_attempt = true;
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

