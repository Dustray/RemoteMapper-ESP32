#include "key_config_storage.h"
#include "log/app_log.h"
#include <Preferences.h>
#include <ArduinoJson.h>

void key_config_storage_init(key_mapper_engine_t *engine) {
    if (!key_config_storage_load(engine)) {
        key_engine_load_defaults(engine);
        app_log("KEYMAP", "Loaded safe factory defaults (%u bindings)", (unsigned int)engine->binding_count);
    } else {
        app_log("KEYMAP", "Loaded custom keymap from NVS (%u bindings)", (unsigned int)engine->binding_count);
    }
}

String key_config_to_json(const key_mapper_engine_t *engine) {
    if (!engine) return "{\"bindings\":[]}";

    JsonDocument doc;
    JsonArray arr = doc["bindings"].to<JsonArray>();

    for (size_t i = 0; i < engine->binding_count; i++) {
        const key_binding_t *b = &engine->bindings[i];
        JsonObject obj = arr.add<JsonObject>();

        obj["source_vk"] = b->source_vk;
        
        // Click action
        if (b->has_click) {
            obj["has_click"] = true;
            obj["click_type"] = (int)b->click_action.type;
            if (b->click_action.modifier != 0) obj["click_mod"] = b->click_action.modifier;
            if (b->click_action.key_code != 0) obj["click_key"] = b->click_action.key_code;
            if (b->click_action.consumer_code != 0) obj["click_cons"] = b->click_action.consumer_code;
        }

        // Long action - omit unused fields if false to keep NVS size tiny
        if (b->has_long) {
            obj["has_long"] = true;
            obj["long_ms"] = b->long_ms;
            obj["long_type"] = (int)b->long_action.type;
            if (b->long_action.modifier != 0) obj["long_mod"] = b->long_action.modifier;
            if (b->long_action.key_code != 0) obj["long_key"] = b->long_action.key_code;
            if (b->long_action.consumer_code != 0) obj["long_cons"] = b->long_action.consumer_code;
        }

        // Double action - omit unused fields if false to keep NVS size tiny
        if (b->has_double) {
            obj["has_double"] = true;
            obj["double_ms"] = b->double_ms;
            obj["double_type"] = (int)b->double_action.type;
            if (b->double_action.modifier != 0) obj["double_mod"] = b->double_action.modifier;
            if (b->double_action.key_code != 0) obj["double_key"] = b->double_action.key_code;
            if (b->double_action.consumer_code != 0) obj["double_cons"] = b->double_action.consumer_code;
        }
    }

    String out;
    serializeJson(doc, out);
    return out;
}

bool key_config_from_json(key_mapper_engine_t *engine, const String &json_str) {
    if (!engine || json_str.length() == 0) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_str);
    if (err) {
        app_log("KEYMAP", "JSON deserialize failed: %s", err.c_str());
        return false;
    }

    JsonArray arr = doc["bindings"].as<JsonArray>();
    if (arr.isNull()) return false;

    engine->binding_count = 0;
    for (JsonObject obj : arr) {
        if (engine->binding_count >= MAX_KEY_BINDINGS) break;

        key_binding_t b;
        memset(&b, 0, sizeof(b));

        b.source_vk = obj["source_vk"] | 0;

        b.has_click = obj["has_click"] | false;
        b.click_action.type = (key_action_type_t)(obj["click_type"] | 0);
        b.click_action.modifier = obj["click_mod"] | 0;
        b.click_action.key_code = obj["click_key"] | 0;
        b.click_action.consumer_code = obj["click_cons"] | 0;

        // Normalize MI_KEY_VOICE_ALT (0x3E) to MI_KEY_VOICE (0x04)
        if (b.source_vk == MI_KEY_VOICE_ALT) {
            b.source_vk = MI_KEY_VOICE;
        }

        // FORCE ACTION_VOICE_HOLD for the Voice key so the mic always works
        if (b.source_vk == MI_KEY_VOICE) {
            b.click_action.type = ACTION_VOICE_HOLD;
        }

        b.has_long = obj["has_long"] | false;
        b.long_ms = obj["long_ms"] | 600;
        b.long_action.type = (key_action_type_t)(obj["long_type"] | 1);
        b.long_action.modifier = obj["long_mod"] | 0;
        b.long_action.key_code = obj["long_key"] | 0;
        b.long_action.consumer_code = obj["long_cons"] | 0;

        b.has_double = obj["has_double"] | false;
        b.double_ms = obj["double_ms"] | 250;
        b.double_action.type = (key_action_type_t)(obj["double_type"] | 1);
        b.double_action.modifier = obj["double_mod"] | 0;
        b.double_action.key_code = obj["double_key"] | 0;
        b.double_action.consumer_code = obj["double_cons"] | 0;

        engine->bindings[engine->binding_count++] = b;
    }

    app_log("KEYMAP", "Updated in-memory keymap (%u bindings)", (unsigned int)engine->binding_count);
    return true;
}

bool key_config_storage_save(key_mapper_engine_t *engine) {
    if (!engine) return false;
    String json = key_config_to_json(engine);

    Preferences prefs;
    if (!prefs.begin("keymap_conf", false)) {
        app_log("KEYMAP", "Failed to open keymap_conf NVS namespace for writing!");
        return false;
    }
    size_t written = prefs.putString("cfg_json", json);
    prefs.end();

    if (written == 0) {
        app_log("KEYMAP", "ERROR: putString to NVS failed (written 0 bytes)!");
        return false;
    }
    app_log("KEYMAP", "Saved keymap to NVS successfully (%u bytes written)", (unsigned int)written);
    return true;
}

bool key_config_storage_load(key_mapper_engine_t *engine) {
    if (!engine) return false;

    Preferences prefs;
    if (!prefs.begin("keymap_conf", true)) {
        key_engine_load_defaults(engine);
        return false;
    }
    String json = prefs.getString("cfg_json", "");
    prefs.end();

    if (json.length() == 0) {
        key_engine_load_defaults(engine);
        return false;
    }
    bool ok = key_config_from_json(engine, json);
    if (!ok || engine->binding_count == 0) {
        app_log("KEYMAP", "NVS keymap is invalid or corrupted -> auto-fallback to safe defaults!");
        key_engine_load_defaults(engine);
        return false;
    }
    return true;
}

void key_config_storage_reset_defaults(key_mapper_engine_t *engine) {
    if (!engine) return;

    Preferences prefs;
    if (prefs.begin("keymap_conf", false)) {
        prefs.remove("cfg_json");
        prefs.end();
    }
    key_engine_load_defaults(engine);
    app_log("KEYMAP", "Reset keymap to factory defaults (%u bindings)", (unsigned int)engine->binding_count);
}

String key_telemetry_to_json(const key_mapper_engine_t *engine) {
    JsonDocument doc;
    if (engine) {
        doc["source_vk"] = engine->last_telemetry.source_vk;
        doc["is_pressed"] = engine->last_telemetry.is_pressed;
        doc["duration_ms"] = engine->last_telemetry.duration_ms;
        doc["action_type"] = engine->last_telemetry.action_type;
        doc["modifier"] = engine->last_telemetry.modifier;
        doc["key_code"] = engine->last_telemetry.key_code;
        doc["consumer_code"] = engine->last_telemetry.consumer_code;
    }
    String out;
    serializeJson(doc, out);
    return out;
}
