#include "usb_composite.h"
#include "audio/audio_pipeline.h"
#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

static USBHIDKeyboard        s_keyboard;
static USBHIDConsumerControl s_consumer;
static bool                  s_usb_ready = false;

extern "C" {

void usb_composite_init(void) {
    USB.productName("RemoteMapper Wireless Mic & Controller");
    USB.manufacturerName("RemoteMapper");
    USB.serialNumber("RM-ESP32S3-001");
    
    s_keyboard.begin();
    s_consumer.begin();
    USB.begin();
    s_usb_ready = true;
}

void usb_composite_task(void) {
    // Handled by native ESP32-S3 USB stack
    usb_audio_task();
}

bool usb_hid_keyboard_press(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    KeyReport report = {0};
    report.modifiers = modifier;
    report.keys[0] = keycode;
    s_keyboard.sendReport(&report);
    return true;
}

bool usb_hid_keyboard_release(void) {
    if (!s_usb_ready) return false;
    s_keyboard.releaseAll();
    return true;
}

bool usb_hid_keyboard_tap(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    usb_hid_keyboard_press(modifier, keycode);
    delay(12);
    usb_hid_keyboard_release();
    return true;
}

bool usb_hid_consumer_press(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    s_consumer.press(usage_code);
    return true;
}

bool usb_hid_consumer_release(void) {
    if (!s_usb_ready) return false;
    s_consumer.release();
    return true;
}

bool usb_hid_consumer_tap(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    s_consumer.press(usage_code);
    delay(10);
    s_consumer.release();
    return true;
}

void usb_hid_dispatch_action(const key_action_t *action) {
    if (!action) return;

    switch (action->type) {
        case ACTION_KEYBOARD_TAP:
            usb_hid_keyboard_tap(action->modifier, action->key_code);
            break;
        case ACTION_KEYBOARD_HOLD:
            usb_hid_keyboard_press(action->modifier, action->key_code);
            break;
        case ACTION_KEYBOARD_RELEASE:
            usb_hid_keyboard_release();
            break;
        case ACTION_CONSUMER_TAP:
            usb_hid_consumer_tap(action->consumer_code);
            break;
        case ACTION_CONSUMER_HOLD:
            usb_hid_consumer_press(action->consumer_code);
            break;
        case ACTION_CONSUMER_RELEASE:
            usb_hid_consumer_release();
            break;
        case ACTION_VOICE_HOLD:
            // Hold Voice Hotkey (Default RAlt + Comma) and start audio session
            audio_pipeline_start_session(&g_audio_pipeline, 0);
            usb_hid_keyboard_press(DEFAULT_VOICE_MODIFIER, DEFAULT_VOICE_KEY);
            break;
        case ACTION_VOICE_RELEASE:
            // Release Voice Hotkey and stop audio session
            usb_hid_keyboard_release();
            audio_pipeline_stop_session(&g_audio_pipeline);
            break;
        default:
            break;
    }
}

void usb_audio_task(void) {
    if (!g_audio_pipeline.active) return;
    
    // In Isochronous mode, pull 16 samples (1ms @ 16kHz) from ring buffer
    int16_t usb_frame_samples[16];
    audio_pipeline_read_for_usb(&g_audio_pipeline, usb_frame_samples, 16);
}

} // extern "C"
