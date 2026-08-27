#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================
// 1. Xiaomi Remote Physical Key Codes (HOGP / HID raw bytes)
// ==========================================
#define MI_KEY_VOL_UP       0x80    // Android Volume Up (kbdhid drops this on Windows)
#define MI_KEY_VOL_DOWN     0x81    // Android Volume Down (kbdhid drops this on Windows)
#define MI_KEY_BACK         0xF1    // Android Back key (kbdhid drops this on Windows)
#define MI_KEY_POWER        0xFF    // Power button (or 0x66)
#define MI_KEY_POWER_ALT    0x66
#define MI_KEY_HOME         0x24    // Home button (or 0x4A)
#define MI_KEY_HOME_ALT     0x4A
#define MI_KEY_MENU         0x5D    // Menu button (or 0x65)
#define MI_KEY_MENU_ALT     0x65
#define MI_KEY_TV           0xC0    // Live / TV button (or 0x35)
#define MI_KEY_TV_ALT       0x35
#define MI_KEY_UP           0x52    // D-Pad Up
#define MI_KEY_DOWN         0x51    // D-Pad Down
#define MI_KEY_LEFT         0x50    // D-Pad Left
#define MI_KEY_RIGHT        0x4F    // D-Pad Right
#define MI_KEY_OK           0x28    // OK / Enter

// ==========================================
// 2. USB HID Keyboard Modifier Bitmasks
// ==========================================
#define USB_MOD_NONE        0x00
#define USB_MOD_LCTRL       0x01
#define USB_MOD_LSHIFT      0x02
#define USB_MOD_LALT        0x04
#define USB_MOD_LGUI        0x08    // Left Win Key
#define USB_MOD_RCTRL       0x10
#define USB_MOD_RSHIFT      0x20
#define USB_MOD_RALT        0x40    // Right Alt Key (AltGr)
#define USB_MOD_RGUI        0x80    // Right Win Key

// ==========================================
// 3. Standard USB HID Keyboard Key Codes
// ==========================================
#define USB_KEY_NONE        0x00
#define USB_KEY_A           0x04
#define USB_KEY_D           0x07    // 'D' (for Win+D Show Desktop)
#define USB_KEY_RETURN      0x28    // Enter
#define USB_KEY_ESCAPE      0x29    // Esc
#define USB_KEY_BACKSPACE   0x2A
#define USB_KEY_TAB         0x2B    // Tab (for Alt+Tab Task Switch)
#define USB_KEY_SPACE       0x2C    // Spacebar
#define USB_KEY_F5          0x3E    // F5 Refresh
#define USB_KEY_F8          0x41    // F8 Voice IME hotkey
#define USB_KEY_RIGHT       0x4F    // Arrow Right
#define USB_KEY_LEFT        0x50    // Arrow Left
#define USB_KEY_DOWN        0x51    // Arrow Down
#define USB_KEY_UP          0x52    // Arrow Up
#define USB_KEY_COMMA       0x36    // ',' (for RAlt+, WeChat IME hotkey)

// ==========================================
// 4. USB HID Consumer Control Usage Codes (16-bit)
// ==========================================
#define USB_CONSUMER_NONE               0x0000
#define USB_CONSUMER_POWER              0x0030
#define USB_CONSUMER_RESET              0x0031
#define USB_CONSUMER_SLEEP              0x0032
#define USB_CONSUMER_PLAY_PAUSE         0x00CD
#define USB_CONSUMER_MUTE               0x00E2
#define USB_CONSUMER_VOLUME_UP          0x00E9  // Standard PC Volume Increment
#define USB_CONSUMER_VOLUME_DOWN        0x00EA  // Standard PC Volume Decrement
#define USB_CONSUMER_AL_CONSUMER_CTRL   0x0182
#define USB_CONSUMER_AC_BACK            0x0224  // Standard PC Browser / App Back
#define USB_CONSUMER_AC_FORWARD         0x0225
#define USB_CONSUMER_AC_REFRESH         0x0227
#define USB_CONSUMER_AC_BOOKMARKS       0x022A
#define USB_CONSUMER_AC_PAN             0x0238

#ifdef __cplusplus
}
#endif
