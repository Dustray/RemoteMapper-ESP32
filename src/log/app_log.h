#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

#define MAX_LOG_LINES 60
#define MAX_LOG_LINE_LEN 160

#ifdef __cplusplus
extern "C" {
#endif

void app_log_init(void);
void app_log(const char* tag, const char* format, ...);
String app_log_get_json(void);
void app_log_clear(void);

#ifdef __cplusplus
}
#endif
