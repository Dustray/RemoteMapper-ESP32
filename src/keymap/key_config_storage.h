#pragma once

#include "key_state_machine.h"
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void key_config_storage_init(key_mapper_engine_t *engine);
bool key_config_storage_save(key_mapper_engine_t *engine);
bool key_config_storage_load(key_mapper_engine_t *engine);
void key_config_storage_reset_defaults(key_mapper_engine_t *engine);

#ifdef __cplusplus
}
#endif

String key_config_to_json(const key_mapper_engine_t *engine);
bool key_config_from_json(key_mapper_engine_t *engine, const String &json_str);
String key_telemetry_to_json(const key_mapper_engine_t *engine);
