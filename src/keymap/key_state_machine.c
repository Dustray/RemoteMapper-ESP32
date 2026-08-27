#include "key_state_machine.h"
#include <string.h>

static int find_binding_index(const key_mapper_engine_t *engine, uint8_t raw_key) {
    if (!engine) return -1;
    for (size_t i = 0; i < engine->binding_count; i++) {
        if (engine->bindings[i].source_vk == raw_key) {
            return (int)i;
        }
        // Handle alternate codes
        if (raw_key == MI_KEY_POWER_ALT && engine->bindings[i].source_vk == MI_KEY_POWER) return (int)i;
        if (raw_key == MI_KEY_HOME_ALT && engine->bindings[i].source_vk == MI_KEY_HOME) return (int)i;
        if (raw_key == MI_KEY_MENU_ALT && engine->bindings[i].source_vk == MI_KEY_MENU) return (int)i;
        if (raw_key == MI_KEY_TV_ALT && engine->bindings[i].source_vk == MI_KEY_TV) return (int)i;
    }
    return -1;
}

static void emit_action(key_mapper_engine_t *engine, const key_action_t *action) {
    if (engine && engine->output_cb && action && action->type != ACTION_NONE) {
        engine->output_cb(action);
    }
}

void key_engine_load_defaults(key_mapper_engine_t *engine) {
    if (!engine) return;
    engine->binding_count = 0;

    // 1. Volume Up: 0x80 -> Consumer Volume Up with fast repeat
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_VOL_UP;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_UP };
        b.has_repeat = true;
        b.repeat_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_UP };
        b.repeat_delay_ms = 350;
        b.repeat_interval_ms = 70;
        engine->bindings[engine->binding_count++] = b;
    }

    // 2. Volume Down: 0x81 -> Consumer Volume Down with fast repeat
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_VOL_DOWN;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_DOWN };
        b.has_repeat = true;
        b.repeat_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_DOWN };
        b.repeat_delay_ms = 350;
        b.repeat_interval_ms = 70;
        engine->bindings[engine->binding_count++] = b;
    }

    // 3. Back: 0xF1 -> Consumer AC Back (Browser / App Back)
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_BACK;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_AC_BACK };
        engine->bindings[engine->binding_count++] = b;
    }

    // 4. Power: 0xFF -> Click: Alt+Tab, Long: Sleep
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_POWER;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_LALT, USB_KEY_TAB, 0 };
        b.has_long = true;
        b.long_ms = 600;
        b.long_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_SLEEP };
        engine->bindings[engine->binding_count++] = b;
    }

    // 5. Home: 0x24 -> Click: Win+D (Show Desktop)
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_HOME;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_LGUI, USB_KEY_D, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 6. Menu: 0x5D -> Click: Space (Play/Pause)
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_MENU;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_NONE, USB_KEY_SPACE, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 7. TV: 0xC0 -> Click: F8
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_TV;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_NONE, USB_KEY_F8, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 8. D-Pad Up: 0x52 -> Keyboard Up
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_UP;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_UP, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 9. D-Pad Down: 0x51 -> Keyboard Down
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_DOWN;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_DOWN, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 10. D-Pad Left: 0x50 -> Keyboard Left
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_LEFT;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_LEFT, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 11. D-Pad Right: 0x4F -> Keyboard Right
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_RIGHT;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_RIGHT, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 12. OK: 0x28 -> Keyboard Return
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_OK;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_RETURN, 0 };
        engine->bindings[engine->binding_count++] = b;
    }
}

void key_engine_init(key_mapper_engine_t *engine, key_output_callback_t cb) {
    if (!engine) return;
    memset(engine, 0, sizeof(key_mapper_engine_t));
    engine->output_cb = cb;
    key_engine_load_defaults(engine);
}

void key_engine_feed_key(key_mapper_engine_t *engine, uint8_t raw_key_code, bool is_pressed, uint32_t now_ms) {
    if (!engine) return;

    int idx = find_binding_index(engine, raw_key_code);
    if (idx < 0) return;

    key_binding_t *b = &engine->bindings[idx];
    key_slot_state_t *st = &engine->states[idx];

    if (is_pressed) {
        if (!st->is_pressed) {
            st->is_pressed = true;
            st->press_timestamp = now_ms;
            st->long_fired = false;
            st->press_count++;

            // If key has a direct HOLD action without long/double ambiguity, fire immediately
            if (!b->has_long && !b->has_double && b->has_click && b->click_action.type == ACTION_KEYBOARD_HOLD) {
                emit_action(engine, &b->click_action);
            } else if (b->has_repeat) {
                // Initialize repeat
                emit_action(engine, &b->click_action);
                st->next_repeat_timestamp = now_ms + b->repeat_delay_ms;
            }
        }
    } else {
        // Key Released
        if (st->is_pressed) {
            st->is_pressed = false;
            st->release_timestamp = now_ms;

            if (b->has_click && b->click_action.type == ACTION_KEYBOARD_HOLD) {
                // Release hold
                key_action_t rel = b->click_action;
                rel.type = ACTION_KEYBOARD_RELEASE;
                emit_action(engine, &rel);
            } else if (!st->long_fired && !b->has_repeat) {
                if (b->has_double) {
                    if (st->press_count >= 2) {
                        emit_action(engine, &b->double_action);
                        st->press_count = 0;
                        st->waiting_double = false;
                    } else {
                        st->waiting_double = true;
                    }
                } else if (b->has_click) {
                    emit_action(engine, &b->click_action);
                    st->press_count = 0;
                }
            }
        }
    }
}

void key_engine_tick(key_mapper_engine_t *engine, uint32_t now_ms) {
    if (!engine) return;

    for (size_t i = 0; i < engine->binding_count; i++) {
        key_binding_t *b = &engine->bindings[i];
        key_slot_state_t *st = &engine->states[i];

        // 1. Long Press check
        if (st->is_pressed && b->has_long && !st->long_fired) {
            uint32_t held_duration = now_ms - st->press_timestamp;
            if (held_duration >= b->long_ms) {
                st->long_fired = true;
                emit_action(engine, &b->long_action);
            }
        }

        // 2. Repeat check
        if (st->is_pressed && b->has_repeat) {
            if (now_ms >= st->next_repeat_timestamp) {
                emit_action(engine, &b->repeat_action);
                st->next_repeat_timestamp = now_ms + b->repeat_interval_ms;
            }
        }

        // 3. Double click timeout check
        if (!st->is_pressed && st->waiting_double && b->has_double) {
            uint32_t since_release = now_ms - st->release_timestamp;
            if (since_release >= b->double_ms) {
                // Double-click window expired -> emit single click
                if (b->has_click) {
                    emit_action(engine, &b->click_action);
                }
                st->waiting_double = false;
                st->press_count = 0;
            }
        }
    }
}
