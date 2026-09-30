#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

#define CLOCK_SYSCLK_HZ 72000000U
#define CLOCK_HCLK_HZ 72000000U
#define CLOCK_PCLK1_HZ 36000000U
#define CLOCK_PCLK2_HZ 72000000U
#define CLOCK_ADC_HZ 12000000U

void clock_init(void);

#endif
