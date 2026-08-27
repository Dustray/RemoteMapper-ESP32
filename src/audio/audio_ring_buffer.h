#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t *buffer;
    size_t   capacity;      // Must be power of 2
    size_t   mask;          // capacity - 1
    volatile size_t head;   // Write index
    volatile size_t tail;   // Read index
} audio_ring_buffer_t;

/**
 * @brief Initialize ring buffer with allocated storage
 * @param rb Ring buffer pointer
 * @param storage Pre-allocated array of int16_t samples
 * @param capacity Number of samples (must be power of 2, e.g. 1024, 2048, 4096)
 * @return true if valid power of 2, false otherwise
 */
bool audio_ring_buffer_init(audio_ring_buffer_t *rb, int16_t *storage, size_t capacity);

/**
 * @brief Get available samples for reading
 */
size_t audio_ring_buffer_available_read(const audio_ring_buffer_t *rb);

/**
 * @brief Get available free sample slots for writing
 */
size_t audio_ring_buffer_available_write(const audio_ring_buffer_t *rb);

/**
 * @brief Write samples into ring buffer
 * @return Number of samples actually written
 */
size_t audio_ring_buffer_write(audio_ring_buffer_t *rb, const int16_t *samples, size_t count);

/**
 * @brief Read samples from ring buffer
 * @return Number of samples actually read
 */
size_t audio_ring_buffer_read(audio_ring_buffer_t *rb, int16_t *out_samples, size_t count);

/**
 * @brief Clear ring buffer contents
 */
void audio_ring_buffer_clear(audio_ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif
