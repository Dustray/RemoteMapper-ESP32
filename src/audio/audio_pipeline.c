#include "audio_pipeline.h"
#include <string.h>

audio_pipeline_t g_audio_pipeline;

void audio_pipeline_init(audio_pipeline_t *pipeline) {
    if (!pipeline) return;
    memset(pipeline, 0, sizeof(audio_pipeline_t));
    adpcm_init_state(&pipeline->adpcm);
    audio_filter_init(&pipeline->filter);
    audio_agc_init(&pipeline->agc);
    audio_ring_buffer_init(&pipeline->ring_buf, pipeline->ring_storage, AUDIO_RING_BUFFER_SIZE);
    pipeline->active = false;
    pipeline->session_id = 0;
    pipeline->total_frames_decoded = 0;
    pipeline->total_samples_pushed = 0;
}

void audio_pipeline_start_session(audio_pipeline_t *pipeline, uint8_t session_id) {
    if (!pipeline) return;
    adpcm_init_state(&pipeline->adpcm);
    audio_filter_init(&pipeline->filter);
    audio_agc_reset(&pipeline->agc);
    audio_ring_buffer_clear(&pipeline->ring_buf);
    pipeline->session_id = session_id;
    pipeline->active = true;
    pipeline->total_frames_decoded = 0;
    pipeline->total_samples_pushed = 0;
}

void audio_pipeline_sync(audio_pipeline_t *pipeline, int16_t predictor, int8_t step_index) {
    if (!pipeline) return;
    adpcm_sync_state(&pipeline->adpcm, predictor, step_index);
}

size_t audio_pipeline_feed_adpcm(audio_pipeline_t *pipeline, const uint8_t *adpcm_bytes, size_t len) {
    if (!pipeline || !adpcm_bytes || len == 0 || !pipeline->active) return 0;

    size_t samples_decoded = adpcm_decode_frame(&pipeline->adpcm, adpcm_bytes, len, pipeline->temp_pcm);
    if (samples_decoded == 0) return 0;

    // 1. Declip
    audio_filter_declip(&pipeline->filter, pipeline->temp_pcm, samples_decoded, DECLIP_THRESHOLD);

    // 2. 3-Tap Triangle FIR Lowpass
    audio_filter_lowpass(&pipeline->filter, pipeline->temp_pcm, samples_decoded);

    // 3. Dynamic AGC + Soft Clip
    audio_agc_process(&pipeline->agc, pipeline->temp_pcm, samples_decoded);

    // 4. Enqueue into ring buffer
    size_t written = audio_ring_buffer_write(&pipeline->ring_buf, pipeline->temp_pcm, samples_decoded);

    pipeline->total_frames_decoded++;
    pipeline->total_samples_pushed += written;
    return written;
}

size_t audio_pipeline_read_for_usb(audio_pipeline_t *pipeline, int16_t *out_pcm, size_t sample_count) {
    if (!pipeline || !out_pcm || sample_count == 0) return 0;

    if (!pipeline->active) {
        // Feed silence when idle
        memset(out_pcm, 0, sample_count * sizeof(int16_t));
        return sample_count;
    }

    size_t read_count = audio_ring_buffer_read(&pipeline->ring_buf, out_pcm, sample_count);

    // If underrun, pad remainder with silence
    if (read_count < sample_count) {
        memset(&out_pcm[read_count], 0, (sample_count - read_count) * sizeof(int16_t));
    }

    return sample_count;
}

void audio_pipeline_stop_session(audio_pipeline_t *pipeline) {
    if (!pipeline) return;
    pipeline->active = false;
    audio_ring_buffer_clear(&pipeline->ring_buf);
}
