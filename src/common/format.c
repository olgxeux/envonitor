#include "format.h"

static bool format_uint32(uint32_t value, char *buffer, uint32_t capacity)
{
    char reversed[10];
    uint32_t count = 0U;

    if (buffer == 0 || capacity == 0U)
    {
        return false;
    }

    if (value == 0U)
    {
        if (capacity < 2U)
        {
            return false;
        }

        buffer[0] = '0';
        buffer[1] = '\0';

        return true;
    }

    while (value > 0U)
    {
        reversed[count++] = (char)('0' + value % 10U);

        value /= 10U;
    }

    if (capacity <= count)
    {
        return false;
    }

    for (uint32_t i = 0U; i < count; ++i)
    {
        buffer[i] = reversed[count - i - 1U];
    }

    buffer[count] = '\0';

    return true;
}

bool format_int32(int32_t value, char *buffer, uint32_t capacity)
{
    if (buffer == 0 || capacity == 0U)
    {
        return false;
    }

    if (value >= 0)
    {
        return format_uint32((uint32_t)value, buffer, capacity);
    }

    if (capacity < 2U)
    {
        return false;
    }

    buffer[0] = '-';

    uint32_t magnitude = (uint32_t)(-(int64_t)value);

    return format_uint32(magnitude, &buffer[1], capacity - 1U);
}

bool format_float_1dp(float value, char *buffer, uint32_t capacity)
{
    if (buffer == 0 || capacity == 0U)
    {
        return false;
    }

    uint32_t index = 0U;

    if (value < 0.0f)
    {
        if (capacity < 2U)
        {
            return false;
        }

        buffer[index++] = '-';
        value = -value;
    }

    uint32_t scaled = (uint32_t)(value * 10.0f + 0.5f);

    uint32_t whole = scaled / 10U;
    uint32_t fraction = scaled % 10U;

    char whole_buffer[11];

    if (!format_uint32(whole, whole_buffer, sizeof(whole_buffer)))
    {
        return false;
    }

    uint32_t whole_index = 0U;

    while (whole_buffer[whole_index] != '\0')
    {
        if (index + 1U >= capacity)
        {
            return false;
        }

        buffer[index++] = whole_buffer[whole_index++];
    }

    if (index + 2U >= capacity)
    {
        return false;
    }

    buffer[index++] = '.';
    buffer[index++] = (char)('0' + fraction);

    buffer[index] = '\0';

    return true;
}
