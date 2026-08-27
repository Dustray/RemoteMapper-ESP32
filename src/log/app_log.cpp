#include "app_log.h"
#include <ArduinoJson.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char   s_log_lines[MAX_LOG_LINES][MAX_LOG_LINE_LEN];
static size_t s_log_head = 0;
static size_t s_log_count = 0;
static portMUX_TYPE s_log_mux = portMUX_INITIALIZER_UNLOCKED;

void app_log_init(void) {
    s_log_head = 0;
    s_log_count = 0;
    memset(s_log_lines, 0, sizeof(s_log_lines));
}

void app_log(const char* tag, const char* format, ...) {
    char msg_buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(msg_buf, sizeof(msg_buf), format, args);
    va_end(args);

    uint32_t now_ms = millis();
    uint32_t sec = now_ms / 1000;
    uint32_t ms = now_ms % 1000;

    char full_line[MAX_LOG_LINE_LEN];
    snprintf(full_line, sizeof(full_line), "[%04u.%03u] [%s] %s", (unsigned int)sec, (unsigned int)ms, tag, msg_buf);

    // 1. Output to Serial
    Serial.println(full_line);

    // 2. Store to circular buffer
    taskENTER_CRITICAL(&s_log_mux);
    strncpy(s_log_lines[s_log_head], full_line, MAX_LOG_LINE_LEN - 1);
    s_log_lines[s_log_head][MAX_LOG_LINE_LEN - 1] = '\0';
    s_log_head = (s_log_head + 1) % MAX_LOG_LINES;
    if (s_log_count < MAX_LOG_LINES) {
        s_log_count++;
    }
    taskEXIT_CRITICAL(&s_log_mux);
}

String app_log_get_json(void) {
    JsonDocument doc;
    JsonArray arr = doc["logs"].to<JsonArray>();

    taskENTER_CRITICAL(&s_log_mux);
    size_t start_idx = (s_log_count < MAX_LOG_LINES) ? 0 : s_log_head;
    for (size_t i = 0; i < s_log_count; i++) {
        size_t idx = (start_idx + i) % MAX_LOG_LINES;
        arr.add(s_log_lines[idx]);
    }
    taskEXIT_CRITICAL(&s_log_mux);

    String out;
    serializeJson(doc, out);
    return out;
}

void app_log_clear(void) {
    taskENTER_CRITICAL(&s_log_mux);
    s_log_head = 0;
    s_log_count = 0;
    taskEXIT_CRITICAL(&s_log_mux);
}
