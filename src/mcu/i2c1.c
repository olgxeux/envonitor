#include "i2c1.h"
#include "clock.h"
#include "time.h"

#include "stm32f1xx.h"

#define I2C1_CLOCK_HZ 100000U

#define I2C1_PCLK_MHZ (CLOCK_PCLK1_HZ / 1000000U)
#define I2C1_CCR_VALUE (CLOCK_PCLK1_HZ / (2U * I2C1_CLOCK_HZ))
#define I2C1_TRISE_VALUE (I2C1_PCLK_MHZ + 1U)

#define I2C1_TIMEOUT_MS 5U

#define I2C1_ERROR_FLAGS (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR | I2C_SR1_TIMEOUT)

static void i2c1_gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    GPIOB->CRL &= ~(GPIO_CRL_MODE6 | GPIO_CRL_CNF6);
    GPIOB->CRL |= GPIO_CRL_MODE6 | GPIO_CRL_CNF6;

    GPIOB->CRL &= ~(GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
    GPIOB->CRL |= GPIO_CRL_MODE7 | GPIO_CRL_CNF7;
}

static void i2c1_clear_addr(void)
{
    volatile uint32_t temp;

    temp = I2C1->SR1;
    temp = I2C1->SR2;

    (void)temp;
}

static void i2c1_clear_errors(void)
{
    I2C1->SR1 &= ~I2C1_ERROR_FLAGS;
}

static bool i2c1_wait_sr1(uint32_t flag)
{
    uint32_t start_ms = time_get_ms();

    while (1)
    {
        uint32_t status = I2C1->SR1;

        if (status & I2C1_ERROR_FLAGS)
        {
            return false;
        }

        if (status & flag)
        {
            return true;
        }

        if (time_get_ms() - start_ms >= I2C1_TIMEOUT_MS)
        {
            return false;
        }
    }
}

static bool i2c1_wait_bus_free(void)
{
    uint32_t start_ms = time_get_ms();

    while (I2C1->SR2 & I2C_SR2_BUSY)
    {
        if (time_get_ms() - start_ms >= I2C1_TIMEOUT_MS)
        {
            return false;
        }
    }

    return true;
}

static void i2c1_abort(void)
{
    if (I2C1->SR2 & I2C_SR2_MSL)
    {
        I2C1->CR1 |= I2C_CR1_STOP;
    }

    I2C1->CR1 &= ~(I2C_CR1_ACK | I2C_CR1_POS);

    i2c1_clear_errors();
}

static bool i2c1_start_write(uint8_t address)
{
    I2C1->CR1 &= ~(I2C_CR1_ACK | I2C_CR1_POS);
    I2C1->CR1 |= I2C_CR1_START;

    if (!i2c1_wait_sr1(I2C_SR1_SB))
    {
        return false;
    }

    I2C1->DR = (uint8_t)(address << 1U);

    if (!i2c1_wait_sr1(I2C_SR1_ADDR))
    {
        return false;
    }

    i2c1_clear_addr();

    return true;
}

static bool i2c1_start_read(uint8_t address, uint32_t length)
{
    I2C1->CR1 &= ~(I2C_CR1_ACK | I2C_CR1_POS);

    if (length == 2U)
    {
        I2C1->CR1 |= I2C_CR1_ACK | I2C_CR1_POS;
    }
    else if (length > 2U)
    {
        I2C1->CR1 |= I2C_CR1_ACK;
    }

    I2C1->CR1 |= I2C_CR1_START;

    if (!i2c1_wait_sr1(I2C_SR1_SB))
    {
        return false;
    }

    I2C1->DR = (uint8_t)((address << 1U) | 1U);

    if (!i2c1_wait_sr1(I2C_SR1_ADDR))
    {
        return false;
    }

    return true;
}

static bool i2c1_transmit(const uint8_t *data, uint32_t length)
{
    for (uint32_t i = 0U; i < length; ++i)
    {
        if (!i2c1_wait_sr1(I2C_SR1_TXE))
        {
            return false;
        }

        I2C1->DR = data[i];
    }

    return i2c1_wait_sr1(I2C_SR1_BTF);
}

static bool i2c1_receive(uint8_t *data, uint32_t length)
{
    if (length == 1U)
    {
        __disable_irq();

        i2c1_clear_addr();
        I2C1->CR1 |= I2C_CR1_STOP;

        __enable_irq();

        if (!i2c1_wait_sr1(I2C_SR1_RXNE))
        {
            return false;
        }

        data[0] = (uint8_t)I2C1->DR;

        return true;
    }

    if (length == 2U)
    {
        __disable_irq();

        i2c1_clear_addr();
        I2C1->CR1 &= ~I2C_CR1_ACK;

        __enable_irq();

        if (!i2c1_wait_sr1(I2C_SR1_BTF))
        {
            return false;
        }

        __disable_irq();

        I2C1->CR1 |= I2C_CR1_STOP;
        data[0] = (uint8_t)I2C1->DR;

        __enable_irq();

        data[1] = (uint8_t)I2C1->DR;

        uint32_t start_ms = time_get_ms();

        while (I2C1->CR1 & I2C_CR1_STOP)
        {
            if (time_get_ms() - start_ms >= I2C1_TIMEOUT_MS)
            {
                return false;
            }
        }

        I2C1->CR1 &= ~I2C_CR1_POS;

        return true;
    }

    i2c1_clear_addr();

    uint32_t index = 0U;
    uint32_t remaining = length;

    while (remaining > 3U)
    {
        if (!i2c1_wait_sr1(I2C_SR1_RXNE))
        {
            return false;
        }

        data[index++] = (uint8_t)I2C1->DR;
        --remaining;
    }

    if (!i2c1_wait_sr1(I2C_SR1_BTF))
    {
        return false;
    }

    I2C1->CR1 &= ~I2C_CR1_ACK;

    __disable_irq();

    data[index++] = (uint8_t)I2C1->DR;

    I2C1->CR1 |= I2C_CR1_STOP;

    data[index++] = (uint8_t)I2C1->DR;

    __enable_irq();

    if (!i2c1_wait_sr1(I2C_SR1_RXNE))
    {
        return false;
    }

    data[index] = (uint8_t)I2C1->DR;

    return true;
}

void i2c1_init(void)
{
    i2c1_gpio_init();

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    I2C1->CR1 = 0U;

    I2C1->CR2 = I2C1_PCLK_MHZ;
    I2C1->CCR = I2C1_CCR_VALUE;
    I2C1->TRISE = I2C1_TRISE_VALUE;
    I2C1->OAR1 = (1U << 14U);

    I2C1->CR1 = I2C_CR1_PE;
}

bool i2c1_write(uint8_t address, const uint8_t *data, uint32_t length)
{
    if (address > 0x7FU || data == 0 || length == 0U)
    {
        return false;
    }

    if (!i2c1_wait_bus_free())
    {
        return false;
    }

    i2c1_clear_errors();

    if (!i2c1_start_write(address))
    {
        i2c1_abort();
        return false;
    }

    if (!i2c1_transmit(data, length))
    {
        i2c1_abort();
        return false;
    }

    I2C1->CR1 |= I2C_CR1_STOP;

    return true;
}

bool i2c1_read(uint8_t address, uint8_t *data, uint32_t length)
{
    if (address > 0x7FU || data == 0 || length == 0U)
    {
        return false;
    }

    if (!i2c1_wait_bus_free())
    {
        return false;
    }

    i2c1_clear_errors();

    if (!i2c1_start_read(address, length))
    {
        i2c1_abort();
        return false;
    }

    if (!i2c1_receive(data, length))
    {
        i2c1_abort();
        return false;
    }

    return true;
}

bool i2c1_write_read(
    uint8_t address,
    const uint8_t *write_data,
    uint32_t write_length,
    uint8_t *read_data,
    uint32_t read_length
)
{
    if (address > 0x7FU || write_data == 0 || read_data == 0 || write_length == 0U || read_length == 0U)
    {
        return false;
    }

    if (!i2c1_wait_bus_free())
    {
        return false;
    }

    i2c1_clear_errors();

    if (!i2c1_start_write(address))
    {
        i2c1_abort();
        return false;
    }

    if (!i2c1_transmit(write_data, write_length))
    {
        i2c1_abort();
        return false;
    }

    if (!i2c1_start_read(address, read_length))
    {
        i2c1_abort();
        return false;
    }

    if (!i2c1_receive(read_data, read_length))
    {
        i2c1_abort();
        return false;
    }

    return true;
}

bool i2c1_probe(uint8_t address)
{
    if (address > 0x7FU)
    {
        return false;
    }

    if (!i2c1_wait_bus_free())
    {
        return false;
    }

    i2c1_clear_errors();

    if (!i2c1_start_write(address))
    {
        i2c1_abort();
        return false;
    }

    I2C1->CR1 |= I2C_CR1_STOP;

    return true;
}
