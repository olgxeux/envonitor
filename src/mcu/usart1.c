#include "usart1.h"
#include "clock.h"
#include "ring_buffer.h"
#include "stm32f1xx.h"

#define USART1_RX_BUFFER_CAPACITY 128U
#define USART1_TX_BUFFER_CAPACITY 256U

#define USART1_BAUD_RATE 115200U

static char rx_storage[USART1_RX_BUFFER_CAPACITY];
static char tx_storage[USART1_TX_BUFFER_CAPACITY];

static RingBuffer rx_buffer;
static RingBuffer tx_buffer;

static void usart1_gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOA->CRH |= GPIO_CRH_MODE9_1 | GPIO_CRH_CNF9_1;

    GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
    GPIOA->CRH |= GPIO_CRH_CNF10_0;
}

void usart1_init(void)
{
    ring_buffer_init(&rx_buffer, rx_storage, USART1_RX_BUFFER_CAPACITY);
    ring_buffer_init(&tx_buffer, tx_storage, USART1_TX_BUFFER_CAPACITY);

    usart1_gpio_init();

    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;

    USART1->BRR = (CLOCK_PCLK2_HZ + USART1_BAUD_RATE / 2U) / USART1_BAUD_RATE;

    USART1->CR1 = USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE | USART_CR1_UE;

    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
}

bool usart1_try_read_char(char *value)
{
    if (value == 0)
    {
        return false;
    }

    return ring_buffer_try_pop(&rx_buffer, value);
}

bool usart1_try_queue_char(char value)
{
    if (!ring_buffer_try_push(&tx_buffer, value))
    {
        return false;
    }
    USART1->CR1 |= USART_CR1_TXEIE;

    return true;
}

uint32_t usart1_queue_string(const char *string)
{
    if (string == 0)
    {
        return 0U;
    }
    uint32_t queued_count = 0U;
    while (*string != '\0')
    {
        if (!usart1_try_queue_char(*string))
        {
            break;
        }
        ++string;
        ++queued_count;
    }

    return queued_count;
}

void USART1_IRQHandler(void)
{
    uint32_t status = USART1->SR;

    if (status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE))
    {
        char received_char = (char)(uint8_t)USART1->DR;

        if (status & USART_SR_RXNE)
        {
            (void)ring_buffer_try_push(&rx_buffer, received_char);
        }
    }

    if ((status & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE))
    {
        char transmit_char;
        if (ring_buffer_try_pop(&tx_buffer, &transmit_char))
        {
            USART1->DR = (uint8_t)transmit_char;
        }
        else
        {
            USART1->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}
