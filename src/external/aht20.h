#ifndef AHT20_H
#define AHT20_H

#include <stdbool.h>

bool aht20_init(void);

bool aht20_start_measurement(void);

bool aht20_read_measurement(float *temperature, float *humidity);

#endif