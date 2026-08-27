#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t prev_decoded;   // Preserved sample for cross-frame declip continuity
    int16_t last_sample;    // Preserved sample for cross-frame lowpass continuity
} audio_filter_state_t;

/**
 * @brief Initialize or reset audio filter state
 */
void audio_filter_init(audio_filter_state_t *state);

/**
 * @brief Apply Declip algorithm to remove single-sample spikes/glitches
 * @param state Filter continuity state
 * @param samples Array of PCM samples (modified in-place)
 * @param count Number of samples
 * @param threshold Glitch threshold (e.g. 1000)
 */
void audio_filter_declip(audio_filter_state_t *state, int16_t *samples, size_t count, int16_t threshold);

/**
 * @brief Apply 3-Tap triangle FIR lowpass smoothing filter ((prev + 2*cur + next) >> 2)
 * @param state Filter continuity state
 * @param samples Array of PCM samples (modified in-place)
 * @param count Number of samples
 */
void audio_filter_lowpass(audio_filter_state_t *state, int16_t *samples, size_t count);

#ifdef __cplusplus
}
#endif
