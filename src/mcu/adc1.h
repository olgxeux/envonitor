#ifndef ADC1_H
#define ADC1_H

#include <stdbool.h>
#include <stdint.h>

void adc1_init(void);

void adc1_start_conversion(void);

bool adc1_try_read_result(uint16_t *result);

#endif
