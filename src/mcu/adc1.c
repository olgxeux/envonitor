#include "adc1.h"
#include "time.h"

#include "stm32f1xx.h"

static void adc1_gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0);
}

void adc1_init(void)
{
    adc1_gpio_init();

    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    ADC1->CR1 = 0U;
    ADC1->CR2 = 0U;

    ADC1->SMPR2 &= ~ADC_SMPR2_SMP0;
    ADC1->SMPR2 |= ADC_SMPR2_SMP0_2 | ADC_SMPR2_SMP0_0;

    ADC1->SQR1 &= ~ADC_SQR1_L;
    ADC1->SQR3 &= ~ADC_SQR3_SQ1;

    ADC1->CR2 |= ADC_CR2_ADON;
    delay_ms(50);

    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL)
    {
    }

    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL)
    {
    }
}

void adc1_start_conversion(void)
{
    ADC1->CR2 |= ADC_CR2_ADON;
}

bool adc1_try_read_result(uint16_t *result)
{
    if (result == 0)
    {
        return false;
    }

    if (!(ADC1->SR & ADC_SR_EOC))
    {
        return false;
    }

    *result = (uint16_t)ADC1->DR;

    return true;
}
