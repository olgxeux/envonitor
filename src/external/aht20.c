#include "aht20.h"

#include "i2c1.h"
#include "time.h"

#include <stdint.h>

#define AHT20_I2C_ADDRESS 0x38U

#define AHT20_INIT_COMMAND 0xBEU
#define AHT20_INIT_PARAM_1 0x08U
#define AHT20_INIT_PARAM_2 0x00U

#define AHT20_MEASURE_COMMAND 0xACU
#define AHT20_MEASURE_PARAM_1 0x33U
#define AHT20_MEASURE_PARAM_2 0x00U

#define AHT20_BUSY_MASK 0x80U

#define AHT20_POWER_ON_DELAY_MS 40U
#define AHT20_INIT_DELAY_MS 10U

#define AHT20_RAW_SCALE 1048576.0f

bool aht20_init(void)
{
    static const uint8_t init_command[] = {AHT20_INIT_COMMAND, AHT20_INIT_PARAM_1, AHT20_INIT_PARAM_2};

    delay_ms(AHT20_POWER_ON_DELAY_MS);

    if (!i2c1_write(AHT20_I2C_ADDRESS, init_command, sizeof(init_command)))
    {
        return false;
    }

    delay_ms(AHT20_INIT_DELAY_MS);

    return true;
}

bool aht20_start_measurement(void)
{
    static const uint8_t measure_command[] = {AHT20_MEASURE_COMMAND, AHT20_MEASURE_PARAM_1, AHT20_MEASURE_PARAM_2};

    return i2c1_write(AHT20_I2C_ADDRESS, measure_command, sizeof(measure_command));
}

bool aht20_read_measurement(float *temperature, float *humidity)
{
    if (temperature == 0 || humidity == 0)
    {
        return false;
    }

    uint8_t data[6];

    if (!i2c1_read(AHT20_I2C_ADDRESS, data, sizeof(data)))
    {
        return false;
    }

    if (data[0] & AHT20_BUSY_MASK)
    {
        return false;
    }

    uint32_t humidity_raw = ((uint32_t)data[1] << 12U) | ((uint32_t)data[2] << 4U) | ((uint32_t)data[3] >> 4U);
    uint32_t temperature_raw = ((uint32_t)(data[3] & 0x0FU) << 16U) | ((uint32_t)data[4] << 8U) | (uint32_t)data[5];

    *humidity = ((float)humidity_raw * 100.0f) / AHT20_RAW_SCALE;
    *temperature = ((float)temperature_raw * 200.0f) / AHT20_RAW_SCALE - 50.0f;

    return true;
}