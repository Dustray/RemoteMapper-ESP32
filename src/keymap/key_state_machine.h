#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "key_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACTION_NONE = 0,
    ACTION_KEYBOARD_TAP,        // Tap a key or combo (Down + Up)
    ACTION_KEYBOARD_HOLD,       // Press and hold down
    ACTION_KEYBOARD_RELEASE,    // Release key
    ACTION_CONSUMER_TAP,        // Tap a consumer control usage (e.g. Vol Up/Down)
    ACTION_CONSUMER_HOLD,
    ACTION_CONSUMER_RELEASE,
    ACTION_VOICE_HOLD,          // Trigger voice recording + hold hotkey
    ACTION_VOICE_RELEASE        // End voice recording + release hotkey
} key_action_type_t;

typedef struct {
    key_action_type_t type;
    uint8_t           modifier;     // USB_MOD_*
    uint8_t           key_code;     // USB_KEY_*
    uint16_t          consumer_code;// USB_CONSUMER_*
} key_action_t;

typedef struct {
    uint8_t       source_vk;        // MI_KEY_*
    bool          has_click;
    key_action_t  click_action;
    bool          has_long;
    key_action_t  long_action;
    uint16_t      long_ms;          // e.g. 500ms
    bool          has_double;
    key_action_t  double_action;
    uint16_t      double_ms;        // e.g. 250ms
    bool          has_repeat;
    key_action_t  repeat_action;
    uint16_t      repeat_delay_ms;  // e.g. 400ms
    uint16_t      repeat_interval_ms;// e.g. 80ms
} key_binding_t;

typedef struct {
    bool     is_pressed;
    uint32_t press_timestamp;
    uint32_t release_timestamp;
    uint8_t  press_count;
    bool     long_fired;
    uint32_t next_repeat_timestamp;
    bool     waiting_double;
} key_slot_state_t;

typedef struct {
    uint8_t  source_vk;
    bool     is_pressed;
    uint32_t timestamp;
    uint32_t duration_ms;
    uint8_t  action_type;
    uint8_t  modifier;
    uint8_t  key_code;
    uint16_t consumer_code;
} key_event_telemetry_t;

#define MAX_KEY_BINDINGS 16

typedef void (*key_output_callback_t)(const key_action_t *action);

typedef struct {
    key_binding_t         bindings[MAX_KEY_BINDINGS];
    key_slot_state_t      states[MAX_KEY_BINDINGS];
    size_t                binding_count;
    key_output_callback_t output_cb;
    key_event_telemetry_t last_telemetry;
} key_mapper_engine_t;

/**
 * @brief Initialize key mapper engine with default remote mapping table
 */
void key_engine_init(key_mapper_engine_t *engine, key_output_callback_t cb);

/**
 * @brief Load default factory key mapping table
 */
void key_engine_load_defaults(key_mapper_engine_t *engine);

/**
 * @brief Set or update a key binding
 */
bool key_engine_set_binding(key_mapper_engine_t *engine, const key_binding_t *binding);

/**
 * @brief Get binding for a key code
 */
bool key_engine_get_binding(const key_mapper_engine_t *engine, uint8_t source_vk, key_binding_t *out_binding);

/**
 * @brief Feed raw physical key event from BLE HOGP
 */
void key_engine_feed_key(key_mapper_engine_t *engine, uint8_t raw_key_code, bool is_pressed, uint32_t now_ms);

/**
 * @brief Periodic timer tick to evaluate long press, double click timeout, and repeat timers
 */
void key_engine_tick(key_mapper_engine_t *engine, uint32_t now_ms);

/**
 * @brief Forcefully release all active pressed keys and reset state
 */
void key_engine_release_all(key_mapper_engine_t *engine, uint32_t now_ms);

#ifdef __cplusplus
}
#endif
