#ifndef FORMAT_H
#define FORMAT_H

#include <stdbool.h>
#include <stdint.h>

bool format_int32(int32_t value, char *buffer, uint32_t capacity);

bool format_float_1dp(float value, char *buffer, uint32_t capacity);

#endif
