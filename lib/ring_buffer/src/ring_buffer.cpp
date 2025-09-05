#include <cstdint>
#include <stdlib.h>

#include "ring_buffer.h"

void ring_buffer_init(ring_buffer_t *buf, size_t buf_len) {
    buf->buffer = (unsigned char *)malloc(sizeof(char) * buf_len);
    buf->buffer_len = buf_len;
    buf->head = 0;
    buf->tail = 0;
}

void ring_buffer_delete(ring_buffer_t *buf) {
    free(buf->buffer);
    buf->buffer = NULL;
}

uint16_t ring_buffer_available(ring_buffer_t *buf) {
    return ((uint16_t)(buf->buffer_len + buf->head - buf->tail)) % buf->buffer_len;
}

int16_t ring_buffer_read(ring_buffer_t *buf) {
    if (buf->head == buf->tail) {
        return -1;
    } else {
        unsigned char c = buf->buffer[buf->tail];
        buf->tail = (uint16_t)((buf->tail + 1) % buf->buffer_len);
        return c;
    }
}

void ring_buffer_append(ring_buffer_t *buf, unsigned char c) {
    uint16_t i = (buf->head + 1) % buf->buffer_len;

    if (i != buf->tail) {
      buf->buffer[buf->head] = c;
      buf->head = i;
    }
}

int16_t ring_buffer_head(ring_buffer_t *buf) {
    return buf->head;
}

int16_t ring_buffer_tail(ring_buffer_t *buf) {
    return buf->tail;
}
