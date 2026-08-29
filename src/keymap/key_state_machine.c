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

static void emit_action(key_mapper_engine_t *engine, const key_action_t *action, uint8_t source_vk, bool is_down) {
    if (!engine) return;

    // Record last telemetry event
    engine->last_telemetry.source_vk = source_vk;
    engine->last_telemetry.is_pressed = is_down;
    if (action) {
        engine->last_telemetry.action_type = action->type;
        engine->last_telemetry.modifier = action->modifier;
        engine->last_telemetry.key_code = action->key_code;
        engine->last_telemetry.consumer_code = action->consumer_code;
    }

    if (engine->output_cb && action && action->type != ACTION_NONE) {
        engine->output_cb(action);
    }
}

void key_engine_load_defaults(key_mapper_engine_t *engine) {
    if (!engine) return;
    engine->binding_count = 0;

    // 1. Power: 0x66 -> Click: Alt+Tab, Long: Sleep
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

    // 2. Voice: 0x04 -> Voice Hold & Release
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_VOICE;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_VOICE_HOLD, USB_MOD_LGUI, USB_KEY_H, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 3. D-Pad Up: 0x52 -> Keyboard Up
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_UP;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_UP, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 4. D-Pad Down: 0x51 -> Keyboard Down
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_DOWN;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_DOWN, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 5. D-Pad Left: 0x50 -> Keyboard Left
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_LEFT;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_LEFT, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 6. D-Pad Right: 0x4F -> Keyboard Right
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_RIGHT;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_RIGHT, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 7. OK: 0x28 -> Keyboard Return
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_OK;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_RETURN, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 8. Back: 0xF1 -> Consumer AC Back (Browser / App Back)
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_BACK;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_AC_BACK };
        engine->bindings[engine->binding_count++] = b;
    }

    // 9. Home: 0x24 -> Click: Win+D (Show Desktop)
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_HOME;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_LGUI, USB_KEY_D, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 10. Menu: 0x5D -> Click: Space (Play/Pause)
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_MENU;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_NONE, USB_KEY_SPACE, 0 };
        engine->bindings[engine->binding_count++] = b;
    }

    // 11. Volume Up: 0x80 -> Consumer Volume Up with fast repeat
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

    // 12. Volume Down: 0x81 -> Consumer Volume Down with fast repeat
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

    // 13. TV: 0xC0 -> Click: F8
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_TV;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_NONE, USB_KEY_F8, 0 };
        engine->bindings[engine->binding_count++] = b;
    }
}

bool key_engine_set_binding(key_mapper_engine_t *engine, const key_binding_t *binding) {
    if (!engine || !binding) return false;
    int idx = find_binding_index(engine, binding->source_vk);
    if (idx >= 0) {
        engine->bindings[idx] = *binding;
        return true;
    }
    if (engine->binding_count < MAX_KEY_BINDINGS) {
        engine->bindings[engine->binding_count++] = *binding;
        return true;
    }
    return false;
}

bool key_engine_get_binding(const key_mapper_engine_t *engine, uint8_t source_vk, key_binding_t *out_binding) {
    if (!engine || !out_binding) return false;
    int idx = find_binding_index(engine, source_vk);
    if (idx >= 0) {
        *out_binding = engine->bindings[idx];
        return true;
    }
    return false;
}

void key_engine_init(key_mapper_engine_t *engine, key_output_callback_t cb) {
    if (!engine) return;
    memset(engine, 0, sizeof(key_mapper_engine_t));
    engine->output_cb = cb;
    key_engine_load_defaults(engine);
}

void key_engine_feed_key(key_mapper_engine_t *engine, uint8_t raw_key_code, bool is_pressed, uint32_t now_ms) {
    if (!engine) return;

    // Immediately record telemetry on EVERY single button state change
    engine->last_telemetry.source_vk = raw_key_code;
    engine->last_telemetry.is_pressed = is_pressed;
    engine->last_telemetry.timestamp = now_ms;

    int idx = find_binding_index(engine, raw_key_code);
    if (idx < 0) return;

    key_binding_t *b = &engine->bindings[idx];
    key_slot_state_t *s = &engine->states[idx];

    // Populate telemetry action info
    engine->last_telemetry.action_type = b->click_action.type;
    engine->last_telemetry.modifier = b->click_action.modifier;
    engine->last_telemetry.key_code = b->click_action.key_code;
    engine->last_telemetry.consumer_code = b->click_action.consumer_code;

    if (is_pressed) {
        if (!s->is_pressed) {
            s->is_pressed = true;
            s->press_timestamp = now_ms;
            s->long_fired = false;

            if (b->has_repeat) {
                s->next_repeat_timestamp = now_ms + b->repeat_delay_ms;
            }

            if (b->click_action.type == ACTION_KEYBOARD_HOLD || b->click_action.type == ACTION_CONSUMER_HOLD || b->click_action.type == ACTION_VOICE_HOLD) {
                emit_action(engine, &b->click_action, raw_key_code, true);
            }
        }
    } else {
        if (s->is_pressed) {
            s->is_pressed = false;
            s->release_timestamp = now_ms;
            uint32_t duration = now_ms - s->press_timestamp;
            engine->last_telemetry.duration_ms = duration;

            if (b->click_action.type == ACTION_KEYBOARD_HOLD) {
                key_action_t rel = { ACTION_KEYBOARD_RELEASE, 0, 0, 0 };
                emit_action(engine, &rel, raw_key_code, false);
            } else if (b->click_action.type == ACTION_CONSUMER_HOLD) {
                key_action_t rel = { ACTION_CONSUMER_RELEASE, 0, 0, 0 };
                emit_action(engine, &rel, raw_key_code, false);
            } else if (b->click_action.type == ACTION_VOICE_HOLD) {
                key_action_t rel = { ACTION_VOICE_RELEASE, 0, 0, 0 };
                emit_action(engine, &rel, raw_key_code, false);
            } else if (b->has_click && !s->long_fired) {
                if (!b->has_double) {
                    emit_action(engine, &b->click_action, raw_key_code, false);
                } else {
                    s->press_count++;
                    if (s->press_count == 1) {
                        s->waiting_double = true;
                    } else if (s->press_count >= 2) {
                        s->waiting_double = false;
                        s->press_count = 0;
                        emit_action(engine, &b->double_action, raw_key_code, false);
                    }
                }
            }
        }
    }
}

void key_engine_tick(key_mapper_engine_t *engine, uint32_t now_ms) {
    if (!engine) return;

    for (size_t i = 0; i < engine->binding_count; i++) {
        key_binding_t *b = &engine->bindings[i];
        key_slot_state_t *s = &engine->states[i];

        if (s->is_pressed) {
            uint32_t hold_time = now_ms - s->press_timestamp;
            if (b->has_long && !s->long_fired && hold_time >= b->long_ms) {
                s->long_fired = true;
                emit_action(engine, &b->long_action, b->source_vk, true);
            }

            if (b->has_repeat && now_ms >= s->next_repeat_timestamp) {
                emit_action(engine, &b->repeat_action, b->source_vk, true);
                s->next_repeat_timestamp = now_ms + b->repeat_interval_ms;
            }
        } else {
            if (s->waiting_double && (now_ms - s->release_timestamp >= b->double_ms)) {
                s->waiting_double = false;
                s->press_count = 0;
                emit_action(engine, &b->click_action, b->source_vk, false);
            }
        }
    }
}
