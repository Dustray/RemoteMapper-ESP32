#include "usb_composite.h"
#include "uac_microphone.h"
#include "audio/audio_pipeline.h"
#include "log/app_log.h"
#include "led_indicator.h"
#include <Arduino.h>
#include "USB.h"
#include "USBCDC.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

#if !ARDUINO_USB_CDC_ON_BOOT
USBCDC USBSerial;
#endif

static USBHIDKeyboard        s_keyboard;
static USBHIDConsumerControl s_consumer;
static bool                  s_usb_ready = false;

extern "C" {

void usb_composite_init(void) {
    USB.VID(0x303A);
    USB.PID(0x8089);
    USB.productName("RemoteMapper Audio & Remote Bridge");
    USB.manufacturerName("RemoteMapper");
    USB.serialNumber("RM-ESP32S3-MIC02");
    USB.usbClass(0xEF);
    USB.usbSubClass(0x02);
    USB.usbProtocol(0x01); // MISC_PROTOCOL_IAD

#if !ARDUINO_USB_CDC_ON_BOOT
    USBSerial.begin();
#endif

    uac_microphone_init();

    s_keyboard.begin();
    s_consumer.begin();
    USB.begin();
    s_usb_ready = true;
}

void usb_composite_task(void) {
    uac_microphone_task();
}

static SemaphoreHandle_t s_hid_mutex = NULL;

static void hid_lock(void) {
    if (!s_hid_mutex) {
        s_hid_mutex = xSemaphoreCreateMutex();
    }
    if (s_hid_mutex) {
        xSemaphoreTake(s_hid_mutex, portMAX_DELAY);
    }
}

static void hid_unlock(void) {
    if (s_hid_mutex) {
        xSemaphoreGive(s_hid_mutex);
    }
}

bool usb_hid_keyboard_press(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    hid_lock();
    KeyReport report = {0};
    report.modifiers = modifier;
    report.keys[0] = keycode;
    s_keyboard.sendReport(&report);
    hid_unlock();
    return true;
}

bool usb_hid_keyboard_release(void) {
    if (!s_usb_ready) return false;
    hid_lock();
    KeyReport report = {0}; // All zeroes
    s_keyboard.sendReport(&report);
    s_keyboard.releaseAll();
    hid_unlock();
    return true;
}

bool usb_hid_keyboard_tap(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    usb_hid_keyboard_press(modifier, keycode);
    delay(15);
    usb_hid_keyboard_release();
    return true;
}

bool usb_hid_consumer_press(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    hid_lock();
    s_consumer.press(usage_code);
    hid_unlock();
    return true;
}

bool usb_hid_consumer_release(void) {
    if (!s_usb_ready) return false;
    hid_lock();
    s_consumer.release();
    hid_unlock();
    return true;
}

bool usb_hid_consumer_tap(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    hid_lock();
    s_consumer.press(usage_code);
    delay(15);
    s_consumer.release();
    hid_unlock();
    return true;
}

void usb_hid_dispatch_action(const key_action_t *action) {
    if (!action) return;

    app_log("USB_HID", "Emit Action: type=%d, mod=0x%02X, key=0x%02X, cons=0x%04X", 
            action->type, action->modifier, action->key_code, action->consumer_code);

    if (action->type == ACTION_VOICE_HOLD) {
        led_indicator_set(LED_STATE_MIC_STREAMING); // Solid Blue while voice recording
    } else if (action->type == ACTION_VOICE_RELEASE) {
        led_indicator_set(LED_STATE_CONNECTED);     // Solid Green when voice recording ends
    } else {
        led_indicator_trigger_key(false);           // Yellow flash on ordinary HID key actions
    }

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
            // Hold Voice Hotkey and start audio session
            audio_pipeline_start_session(&g_audio_pipeline, 0);
            if (action->modifier != 0 || action->key_code != 0) {
                usb_hid_keyboard_press(action->modifier, action->key_code);
            }
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
    uac_microphone_task();
}

} // extern "C"
