#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_STATE_DISCONNECTED = 0,
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_TALKING
} ble_remote_state_t;

/**
 * @brief Initialize NimBLE Client for Xiaomi Remote
 */
void ble_remote_init(void);

/**
 * @brief Main BLE task tick (handles state machine, watchdogs, and keep-alive packets)
 */
void ble_remote_task(void);

/**
 * @brief Get current BLE connection state
 */
ble_remote_state_t ble_remote_get_state(void);

/**
 * @brief Trigger manual reconnect / re-scan
 */
void ble_remote_trigger_reconnect(void);

#ifdef __cplusplus
}
#endif
