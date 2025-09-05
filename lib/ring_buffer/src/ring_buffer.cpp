#include <cstdint>
#include "ring_buffer.h"

void ring_buffer_init(ring_buffer_t *buf) {
    buf->head = 0;
    buf->tail = 0;
}

uint16_t ring_buffer_available(ring_buffer_t *buf) {
    return ((uint16_t)(RING_BUFFER_SIZE + buf->head - buf->tail)) % RING_BUFFER_SIZE;
}

int16_t ring_buffer_read(ring_buffer_t *buf) {
    // if the head isn't ahead of the tail, we don't have any characters
    if (buf->head == buf->tail) {
        return -1;
    } else {
        unsigned char c = buf->buffer[buf->tail];
        buf->tail = (ring_buffer_index_t)(buf->tail + 1) % RING_BUFFER_SIZE;
        return c;
    }
}

void ring_buffer_append(ring_buffer_t *buf, unsigned char c) {
    ring_buffer_index_t i = (unsigned int)(buf->head + 1) % RING_BUFFER_SIZE;

    // if we should be storing the received character into the location
    // just before the tail (meaning that the head would advance to the
    // current location of the tail), we're about to overflow the buffer
    // and so we don't write the character or advance the head.
    if (i != buf->tail) {
      buf->buffer[buf->head] = c;
      buf->head = i;
    }
}
