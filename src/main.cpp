#include <Arduino.h>
#include "app_config.h"
#include "version.h"
#include "log/app_log.h"
#include "wifi/wifi_manager.h"
#include "web/web_server.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "usb/usb_composite.h"
#include "ble/ble_remote_client.h"
#include "cli/cli_manager.h"

key_mapper_engine_t g_key_engine;

// Task running on Core 0: BLE Central & Audio Decoding
static void ble_task_core0(void* param) {
    app_log("SYSTEM", "BLE & Audio Task started on Core %d", xPortGetCoreID());
    ble_remote_init();

    while (true) {
        ble_remote_task();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup() {
    // 1. Initialize USB Composite Stack (UAC Mic + HID Keyboard + Consumer + CDC)
    usb_composite_init();
    Serial.begin(115200);
    delay(500);

    // 2. Initialize Global Log System
    app_log_init();
    app_log("SYSTEM", "==================================================");
    app_log("SYSTEM", " %s v%s (%s)", FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_TARGET);
    app_log("SYSTEM", " Xiaomi Remote Hardware Bridge (BLE -> USB + Web)");
    app_log("SYSTEM", "==================================================");

    // 3. Initialize Audio Pipeline
    audio_pipeline_init(&g_audio_pipeline);
    app_log("INIT", "Audio Pipeline initialized (16kHz 16-bit Mono UAC 1.0)");

    // 4. Initialize Key Engine with USB HID dispatcher callback
    key_engine_init(&g_key_engine, usb_hid_dispatch_action);
    app_log("INIT", "Key Engine initialized with %u mappings", (unsigned int)g_key_engine.binding_count);

    // 5. Initialize Serial / CDC CLI Manager
    cli_manager_init();

    // 6. Initialize Wi-Fi AP + STA & Captive Portal
    wifi_manager_init();

    // 7. Initialize Embedded Web Server & REST APIs
    web_server_init();

    // 8. Launch BLE Central Task pinned to Core 0
    xTaskCreatePinnedToCore(
        ble_task_core0,
        "ble_audio_task",
        8192,
        NULL,
        PRIO_TASK_BLE,
        NULL,
        TASK_CORE_BLE
    );

    app_log("SYSTEM", "System initialization complete. Web available at http://192.168.4.1 or http://remotemapper.local");
}

void loop() {
    uint32_t now = millis();

    // 1. Service TinyUSB & Audio push
    usb_composite_task();

    // 2. Service Key State Machine timers (long press, double click, repeat)
    key_engine_tick(&g_key_engine, now);

    // 3. Service Wi-Fi & DNS tasks
    wifi_manager_task();

    // 4. Service HTTP Web Server
    web_server_task();

    // 5. Service Serial / WebSerial CLI commands
    cli_manager_task();

    delay(2);
}
