#pragma once

#if !defined(RING_BUFFER_SIZE)
#define RING_BUFFER_SIZE 64
#endif

typedef struct ring_buffer_s ring_buffer_t;
typedef uint16_t ring_buffer_index_t;

struct ring_buffer_s {
    unsigned char buffer[RING_BUFFER_SIZE];
    ring_buffer_index_t head;
    ring_buffer_index_t tail;
};


void ring_buffer_init(ring_buffer_t *buf);
uint16_t ring_buffer_available(ring_buffer_t *buf);
int16_t ring_buffer_read(ring_buffer_t *buf);
void ring_buffer_append(ring_buffer_t *buf, unsigned char c);
