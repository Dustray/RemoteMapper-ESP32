#include "audio_agc.h"
#include <math.h>

void audio_agc_init(audio_agc_t *agc) {
    if (!agc) return;
    agc->peak = 1000.0f;
    agc->target_level = 28000.0f;
    agc->decay_rate = 0.9997f;
    agc->max_gain = 30.0f;
    agc->noise_floor = 200.0f;
}

void audio_agc_reset(audio_agc_t *agc) {
    if (!agc) return;
    agc->peak = 1000.0f;
}

void audio_agc_process(audio_agc_t *agc, int16_t *samples, size_t count) {
    if (!agc || !samples || count == 0) return;

    float peak = agc->peak;
    float target = agc->target_level;
    float decay = agc->decay_rate;
    float max_gain = agc->max_gain;
    float floor_val = agc->noise_floor;

    for (size_t i = 0; i < count; i++) {
        float v = (float)samples[i];
        float a = (v < 0.0f) ? -v : v;

        if (a > peak) {
            peak = a;
        } else {
            peak *= decay;
        }

        float denom = (peak > floor_val) ? peak : floor_val;
        float gain = target / denom;
        if (gain > max_gain) {
            gain = max_gain;
        }

        v *= gain;

        // Soft clip
        if (v > 32767.0f) {
            v = 32767.0f;
        } else if (v < -32768.0f) {
            v = -32768.0f;
        }

        samples[i] = (int16_t)v;
    }

    agc->peak = peak;
}
