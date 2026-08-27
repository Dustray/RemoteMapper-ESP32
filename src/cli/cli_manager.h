#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Serial / CDC CLI manager
 */
void cli_manager_init(void);

/**
 * @brief Process periodic CLI line inputs
 */
void cli_manager_task(void);

#ifdef __cplusplus
}
#endif
