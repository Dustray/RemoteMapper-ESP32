#include "audio_filter.h"
#include <stdlib.h>

void audio_filter_init(audio_filter_state_t *state) {
    if (!state) return;
    state->prev_decoded = 0;
    state->last_sample = 0;
}

void audio_filter_declip(audio_filter_state_t *state, int16_t *samples, size_t count, int16_t threshold) {
    if (!state || !samples || count == 0) return;

    int32_t prev = state->prev_decoded;
    for (size_t i = 0; i < count; i++) {
        int32_t p = (i == 0) ? prev : samples[i - 1];
        int32_t nx = (i == count - 1) ? samples[i] : samples[i + 1];
        int32_t cur = samples[i];

        int32_t dp = abs(cur - p);
        int32_t dn = abs(cur - nx);
        int32_t nd = abs(nx - p);
        int32_t min_d = (dp < dn) ? dp : dn;

        if (dp > threshold && dn > threshold && min_d > nd * 2) {
            samples[i] = (int16_t)((p + nx) / 2);
        }
    }
    state->prev_decoded = samples[count - 1];
}

void audio_filter_lowpass(audio_filter_state_t *state, int16_t *samples, size_t count) {
    if (!state || !samples || count == 0) return;

    int32_t prev = state->last_sample;
    for (size_t i = 0; i < count - 1; i++) {
        int32_t cur = samples[i];
        samples[i] = (int16_t)((prev + (cur * 2) + samples[i + 1]) >> 2);
        prev = cur;
    }
    state->last_sample = samples[count - 1];
}
