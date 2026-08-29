#include "adpcm_decoder.h"

// Standard IMA-ADPCM Step Table (89 entries) matching RemoteMic.cs
static const int16_t STEP_TABLE[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

// Standard IMA-ADPCM Index Adjustment Table (8 entries) matching RemoteMic.cs
static const int8_t INDEX_TABLE[8] = {
    -1, -1, -1, -1, 2, 4, 6, 8
};

void adpcm_init_state(adpcm_state_t *state) {
    if (!state) return;
    state->predictor = 0;
    state->step_index = 0;
}

void adpcm_sync_state(adpcm_state_t *state, int16_t predictor, int8_t step_index) {
    if (!state) return;
    state->predictor = (int32_t)predictor;
    if (step_index < 0) step_index = 0;
    if (step_index > 88) step_index = 88;
    state->step_index = step_index;
}

int16_t adpcm_decode_nibble(adpcm_state_t *state, uint8_t nibble) {
    int32_t step = STEP_TABLE[state->step_index];
    int32_t diff = step >> 3;

    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;

    if (nibble & 8) {
        state->predictor -= diff;
    } else {
        state->predictor += diff;
    }

    // Clamp to 16-bit signed range
    if (state->predictor > 32767) {
        state->predictor = 32767;
    } else if (state->predictor < -32768) {
        state->predictor = -32768;
    }

    // Update step index
    state->step_index += INDEX_TABLE[nibble & 7];
    if (state->step_index < 0) state->step_index = 0;
    if (state->step_index > 88) state->step_index = 88;

    return (int16_t)state->predictor;
}

size_t adpcm_decode_frame(adpcm_state_t *state, const uint8_t *in_data, size_t in_bytes, int16_t *out_pcm) {
    if (!state || !in_data || !out_pcm || in_bytes == 0) return 0;

    size_t sample_idx = 0;
    for (size_t i = 0; i < in_bytes; i++) {
        uint8_t byte_val = in_data[i];
        // RemoteMic.cs line 458: Hi-nibble first
        out_pcm[sample_idx++] = adpcm_decode_nibble(state, (uint8_t)(byte_val >> 4));
        // RemoteMic.cs line 459: Low-nibble second
        out_pcm[sample_idx++] = adpcm_decode_nibble(state, (uint8_t)(byte_val & 0x0F));
    }

    return sample_idx;
}
