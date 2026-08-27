#include <Arduino.h>
#include "app_config.h"
#include "version.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "usb/usb_composite.h"
#include "ble/ble_remote_client.h"
#include "cli/cli_manager.h"

key_mapper_engine_t g_key_engine;

// Task running on Core 0: BLE Central & Audio Decoding
static void ble_task_core0(void* param) {
    Serial.printf("[SYSTEM] BLE & Audio Task started on Core %d\n", xPortGetCoreID());
    ble_remote_init();

    while (true) {
        ble_remote_task();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("==================================================");
    Serial.printf(" %s v%s (%s)\n", FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_TARGET);
    Serial.println(" Xiaomi Remote Hardware Bridge (BLE -> USB)");
    Serial.println("==================================================");

    // 1. Initialize Audio Pipeline
    audio_pipeline_init(&g_audio_pipeline);
    Serial.println("[INIT] Audio Pipeline initialized (16kHz 16-bit Mono)");

    // 2. Initialize Key Engine with USB HID dispatcher callback
    key_engine_init(&g_key_engine, usb_hid_dispatch_action);
    Serial.printf("[INIT] Key Engine initialized with %u mappings\n", (unsigned int)g_key_engine.binding_count);

    // 3. Initialize USB Composite Stack (UAC Mic + HID Keyboard + Consumer + CDC)
    usb_composite_init();
    Serial.println("[INIT] USB Composite Device ready (UAC Mic + HID Keyboard/Consumer)");

    // 4. Initialize CLI Manager
    cli_manager_init();

    // 5. Launch BLE Central Task pinned to Core 0
    xTaskCreatePinnedToCore(
        ble_task_core0,
        "ble_audio_task",
        8192,
        NULL,
        PRIO_TASK_BLE,
        NULL,
        TASK_CORE_BLE
    );

    Serial.println("[SYSTEM] System initialization complete. Ready!");
}

void loop() {
    uint32_t now = millis();

    // 1. Service TinyUSB & Audio push
    usb_composite_task();

    // 2. Service Key State Machine timers (long press, double click, repeat)
    key_engine_tick(&g_key_engine, now);

    // 3. Service Serial / WebSerial CLI commands
    cli_manager_task();

    delay(2);
}
