#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t predictor;      // Current PCM prediction value [-32768, 32767]
    int8_t  step_index;     // Current step table index [0, 88]
} adpcm_state_t;

/**
 * @brief Initialize or reset ADPCM decoder state to defaults (0, 0)
 */
void adpcm_init_state(adpcm_state_t *state);

/**
 * @brief Reset ADPCM decoder state to specific sync values (e.g. from ATVV AUDIO_SYNC 0x0A)
 */
void adpcm_sync_state(adpcm_state_t *state, int16_t predictor, int8_t step_index);

/**
 * @brief Decode a single 4-bit nibble
 */
int16_t adpcm_decode_nibble(adpcm_state_t *state, uint8_t nibble);

/**
 * @brief Decode a complete ADPCM frame (Hi-nibble first) into 16-bit PCM samples
 * @param state Pointer to ADPCM decoder state
 * @param in_data ADPCM byte array
 * @param in_bytes Number of ADPCM bytes
 * @param out_pcm Output buffer for 16-bit PCM samples (must hold at least in_bytes * 2 samples)
 * @return Number of PCM samples decoded (in_bytes * 2)
 */
size_t adpcm_decode_frame(adpcm_state_t *state, const uint8_t *in_data, size_t in_bytes, int16_t *out_pcm);

#ifdef __cplusplus
}
#endif
