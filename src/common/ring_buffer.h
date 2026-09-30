#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    volatile uint32_t write_index;
    volatile uint32_t read_index;

    char *storage;
    uint32_t capacity;
} RingBuffer;

bool ring_buffer_init(RingBuffer *buffer, char *storage, uint32_t capacity);

bool ring_buffer_try_push(RingBuffer *buffer, char value);

bool ring_buffer_try_pop(RingBuffer *buffer, char *value);

#endif
