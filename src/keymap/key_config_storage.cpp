#include "key_config_storage.h"
#include "log/app_log.h"
#include <Preferences.h>
#include <ArduinoJson.h>

static Preferences s_key_prefs;

void key_config_storage_init(key_mapper_engine_t *engine) {
    s_key_prefs.begin("keymap_conf", false);
    if (!key_config_storage_load(engine)) {
        app_log("KEYMAP", "No custom keymap found, loaded factory defaults (%u bindings)", (unsigned int)engine->binding_count);
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
        obj["has_click"] = b->has_click;
        obj["click_type"] = (int)b->click_action.type;
        obj["click_mod"] = b->click_action.modifier;
        obj["click_key"] = b->click_action.key_code;
        obj["click_cons"] = b->click_action.consumer_code;

        // Long action
        obj["has_long"] = b->has_long;
        obj["long_ms"] = b->long_ms;
        obj["long_type"] = (int)b->long_action.type;
        obj["long_mod"] = b->long_action.modifier;
        obj["long_key"] = b->long_action.key_code;
        obj["long_cons"] = b->long_action.consumer_code;

        // Double action
        obj["has_double"] = b->has_double;
        obj["double_ms"] = b->double_ms;
        obj["double_type"] = (int)b->double_action.type;
        obj["double_mod"] = b->double_action.modifier;
        obj["double_key"] = b->double_action.key_code;
        obj["double_cons"] = b->double_action.consumer_code;
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

        b.has_long = obj["has_long"] | false;
        b.long_ms = obj["long_ms"] | 500;
        b.long_action.type = (key_action_type_t)(obj["long_type"] | 0);
        b.long_action.modifier = obj["long_mod"] | 0;
        b.long_action.key_code = obj["long_key"] | 0;
        b.long_action.consumer_code = obj["long_cons"] | 0;

        b.has_double = obj["has_double"] | false;
        b.double_ms = obj["double_ms"] | 250;
        b.double_action.type = (key_action_type_t)(obj["double_type"] | 0);
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
    s_key_prefs.putString("cfg_json", json);
    app_log("KEYMAP", "Saved keymap to NVS (%u bytes)", (unsigned int)json.length());
    return true;
}

bool key_config_storage_load(key_mapper_engine_t *engine) {
    if (!engine) return false;
    String json = s_key_prefs.getString("cfg_json", "");
    if (json.length() == 0) return false;
    return key_config_from_json(engine, json);
}

void key_config_storage_reset_defaults(key_mapper_engine_t *engine) {
    if (!engine) return;
    s_key_prefs.remove("cfg_json");
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
