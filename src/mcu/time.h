#ifndef TIME_H
#define TIME_H

#include <stdint.h>

void time_init(void);

uint32_t time_get_ms(void);

void delay_ms(uint32_t milliseconds);

#endif
