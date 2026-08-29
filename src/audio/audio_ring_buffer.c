#include "audio_ring_buffer.h"
#include <string.h>

static bool is_power_of_two(size_t n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

bool audio_ring_buffer_init(audio_ring_buffer_t *rb, int16_t *storage, size_t capacity) {
    if (!rb || !storage || !is_power_of_two(capacity)) {
        return false;
    }
    rb->buffer = storage;
    rb->capacity = capacity;
    rb->mask = capacity - 1;
    rb->head = 0;
    rb->tail = 0;
    return true;
}

size_t audio_ring_buffer_available_read(const audio_ring_buffer_t *rb) {
    if (!rb) return 0;
    return rb->head - rb->tail;
}

size_t audio_ring_buffer_available_write(const audio_ring_buffer_t *rb) {
    if (!rb) return 0;
    return rb->capacity - (rb->head - rb->tail);
}

size_t audio_ring_buffer_write(audio_ring_buffer_t *rb, const int16_t *samples, size_t count) {
    if (!rb || !samples || count == 0) return 0;

    size_t free_slots = audio_ring_buffer_available_write(rb);
    size_t to_write = (count < free_slots) ? count : free_slots;
    size_t h = rb->head;

    for (size_t i = 0; i < to_write; i++) {
        rb->buffer[(h + i) & rb->mask] = samples[i];
    }
    __sync_synchronize();
    rb->head = h + to_write;

    return to_write;
}

size_t audio_ring_buffer_read(audio_ring_buffer_t *rb, int16_t *out_samples, size_t count) {
    if (!rb || !out_samples || count == 0) return 0;

    size_t avail = audio_ring_buffer_available_read(rb);
    size_t to_read = (count < avail) ? count : avail;
    size_t t = rb->tail;

    for (size_t i = 0; i < to_read; i++) {
        out_samples[i] = rb->buffer[(t + i) & rb->mask];
    }
    __sync_synchronize();
    rb->tail = t + to_read;

    return to_read;
}

void audio_ring_buffer_clear(audio_ring_buffer_t *rb) {
    if (!rb) return;
    rb->tail = rb->head;
}
