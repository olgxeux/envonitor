#include "time.h"

#include "stm32f1xx.h"

static volatile uint32_t system_time_ms = 0U;

void time_init(void)
{
    system_time_ms = 0U;

    SysTick_Config(SystemCoreClock / 1000U);
}

uint32_t time_get_ms(void)
{
    return system_time_ms;
}

void delay_ms(uint32_t ms)
{
    uint32_t start_time_ms = time_get_ms();
    while ((time_get_ms() - start_time_ms) < ms)
    {
    }
}

void SysTick_Handler(void)
{
    ++system_time_ms;
}
