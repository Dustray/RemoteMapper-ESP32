#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool uac_microphone_init(void);
void uac_microphone_task(void);
bool uac_microphone_is_streaming(void);

#ifdef __cplusplus
}
#endif
