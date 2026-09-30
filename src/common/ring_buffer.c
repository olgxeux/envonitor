#include "ring_buffer.h"

bool ring_buffer_init(RingBuffer *buffer, char *storage, uint32_t capacity)
{
    if (buffer == 0 || storage == 0 || capacity < 2U)
    {
        return false;
    }

    buffer->write_index = 0U;
    buffer->read_index = 0U;
    buffer->storage = storage;
    buffer->capacity = capacity;

    return true;
}

bool ring_buffer_try_push(RingBuffer *buffer, char value)
{
    uint32_t next_write_index = (buffer->write_index + 1U) % buffer->capacity;

    if (next_write_index == buffer->read_index)
    {
        return false;
    }

    buffer->storage[buffer->write_index] = value;
    buffer->write_index = next_write_index;

    return true;
}

bool ring_buffer_try_pop(RingBuffer *buffer, char *value)
{
    if (buffer->write_index == buffer->read_index)
    {
        return false;
    }

    *value = buffer->storage[buffer->read_index];

    buffer->read_index = (buffer->read_index + 1U) % buffer->capacity;

    return true;
}
