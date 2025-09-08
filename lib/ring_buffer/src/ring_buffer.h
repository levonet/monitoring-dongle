#pragma once

typedef struct ring_buffer_s {
    unsigned char *buffer;
    size_t buffer_len;
    uint16_t head;
    uint16_t tail;
} ring_buffer_t;


/*
 * NOTE: Buffer size must have a "power of 2" length.
 */
void ring_buffer_init(ring_buffer_t *buf, size_t buf_len);
void ring_buffer_delete(ring_buffer_t *buf);
uint16_t ring_buffer_available(ring_buffer_t *buf);
int16_t ring_buffer_read(ring_buffer_t *buf);
void ring_buffer_append(ring_buffer_t *buf, unsigned char c);

// For testing only
inline int16_t ring_buffer_head(ring_buffer_t *buf) {return buf->head;}
inline int16_t ring_buffer_tail(ring_buffer_t *buf) {return buf->tail;}
