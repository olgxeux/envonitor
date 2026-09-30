#ifndef BMP280_H
#define BMP280_H

#include <stdbool.h>

bool bmp280_init(void);

bool bmp280_read(float *temperature, float *pressure);

#endif