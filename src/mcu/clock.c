#include "clock.h"

#include "stm32f1xx.h"

void clock_init(void)
{
    RCC->CR |= RCC_CR_HSEON;

    while (!(RCC->CR & RCC_CR_HSERDY))
    {
    }

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_ADCPRE_DIV6 |
                RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;

    RCC->CR |= RCC_CR_PLLON;

    while (!(RCC->CR & RCC_CR_PLLRDY))
    {
    }

    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    SystemCoreClockUpdate();
}
