#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Dump all NVS namespaces and key-values as pretty-printed JSON string
 */
String nvs_manager_dump_json(void);

/**
 * @brief Apply edited JSON string into NVS storage across all namespaces
 * @param json_str JSON string representing namespace dictionary
 * @param err_msg Output error message if parsing/writing fails
 * @return true on success, false on error
 */
bool nvs_manager_apply_json(const String& json_str, String& err_msg);

/**
 * @brief Factory reset: Erase entire NVS partition and reinitialize
 */
bool nvs_manager_erase_all(void);
