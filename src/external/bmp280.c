#include "bmp280.h"

#include "i2c1.h"
#include "time.h"

#include <stdint.h>

#define BMP280_I2C_ADDRESS 0x77U

#define BMP280_REG_CALIBRATION 0x88U
#define BMP280_REG_CHIP_ID 0xD0U
#define BMP280_REG_RESET 0xE0U
#define BMP280_REG_CTRL_MEAS 0xF4U
#define BMP280_REG_CONFIG 0xF5U
#define BMP280_REG_PRESS_MSB 0xF7U

#define BMP280_CHIP_ID 0x58U
#define BMP280_RESET_VALUE 0xB6U

#define BMP280_CONFIG_VALUE 0xA0U
#define BMP280_CTRL_MEAS_VALUE 0x27U

#define BMP280_RESET_DELAY_MS 10U

static uint16_t dig_t1;
static int16_t dig_t2;
static int16_t dig_t3;

static uint16_t dig_p1;
static int16_t dig_p2;
static int16_t dig_p3;
static int16_t dig_p4;
static int16_t dig_p5;
static int16_t dig_p6;
static int16_t dig_p7;
static int16_t dig_p8;
static int16_t dig_p9;

bool bmp280_init(void)
{
    uint8_t register_address = BMP280_REG_CHIP_ID;
    uint8_t chip_id;
    if (!i2c1_write_read(BMP280_I2C_ADDRESS, &register_address, 1U, &chip_id, 1U))
    {
        return false;
    }

    if (chip_id != BMP280_CHIP_ID)
    {
        return false;
    }

    uint8_t reset_command[] = {BMP280_REG_RESET, BMP280_RESET_VALUE};
    if (!i2c1_write(BMP280_I2C_ADDRESS, reset_command, sizeof(reset_command)))
    {
        return false;
    }

    delay_ms(BMP280_RESET_DELAY_MS);

    uint8_t calibration_data[24];
    register_address = BMP280_REG_CALIBRATION;
    if (!i2c1_write_read(BMP280_I2C_ADDRESS, &register_address, 1U, calibration_data, sizeof(calibration_data)))
    {
        return false;
    }

    dig_t1 = (uint16_t)(((uint16_t)calibration_data[1] << 8U) | calibration_data[0]);
    dig_t2 = (int16_t)(((uint16_t)calibration_data[3] << 8U) | calibration_data[2]);
    dig_t3 = (int16_t)(((uint16_t)calibration_data[5] << 8U) | calibration_data[4]);

    dig_p1 = (uint16_t)(((uint16_t)calibration_data[7] << 8U) | calibration_data[6]);
    dig_p2 = (int16_t)(((uint16_t)calibration_data[9] << 8U) | calibration_data[8]);
    dig_p3 = (int16_t)(((uint16_t)calibration_data[11] << 8U) | calibration_data[10]);
    dig_p4 = (int16_t)(((uint16_t)calibration_data[13] << 8U) | calibration_data[12]);
    dig_p5 = (int16_t)(((uint16_t)calibration_data[15] << 8U) | calibration_data[14]);
    dig_p6 = (int16_t)(((uint16_t)calibration_data[17] << 8U) | calibration_data[16]);
    dig_p7 = (int16_t)(((uint16_t)calibration_data[19] << 8U) | calibration_data[18]);
    dig_p8 = (int16_t)(((uint16_t)calibration_data[21] << 8U) | calibration_data[20]);
    dig_p9 = (int16_t)(((uint16_t)calibration_data[23] << 8U) | calibration_data[22]);

    uint8_t config_command[] = {BMP280_REG_CONFIG, BMP280_CONFIG_VALUE};
    if (!i2c1_write(BMP280_I2C_ADDRESS, config_command, sizeof(config_command)))
    {
        return false;
    }

    uint8_t control_command[] = {BMP280_REG_CTRL_MEAS, BMP280_CTRL_MEAS_VALUE};
    return i2c1_write(BMP280_I2C_ADDRESS, control_command, sizeof(control_command));
}

bool bmp280_read(float *temperature, float *pressure)
{
    if (temperature == 0 || pressure == 0)
    {
        return false;
    }

    uint8_t register_address = BMP280_REG_PRESS_MSB;
    uint8_t data[6];
    if (!i2c1_write_read(BMP280_I2C_ADDRESS, &register_address, 1U, data, sizeof(data)))
    {
        return false;
    }

    int32_t pressure_raw = ((int32_t)data[0] << 12U) | ((int32_t)data[1] << 4U) | ((int32_t)data[2] >> 4U);
    int32_t temperature_raw = ((int32_t)data[3] << 12U) | ((int32_t)data[4] << 4U) | ((int32_t)data[5] >> 4U);

    int32_t var1 = ((((temperature_raw >> 3) - ((int32_t)dig_t1 << 1)) * (int32_t)dig_t2) >> 11);
    int32_t temperature_delta = (temperature_raw >> 4) - (int32_t)dig_t1;
    int32_t var2 = (((temperature_delta * temperature_delta) >> 12) * (int32_t)dig_t3) >> 14;
    int32_t t_fine = var1 + var2;
    int32_t temperature_x100 = (t_fine * 5 + 128) >> 8;
    *temperature = (float)temperature_x100 / 100.0f;

    int64_t pressure_var1 = (int64_t)t_fine - 128000;
    int64_t pressure_var2 = pressure_var1 * pressure_var1 * (int64_t)dig_p6;
    pressure_var2 += (pressure_var1 * (int64_t)dig_p5) << 17;
    pressure_var2 += ((int64_t)dig_p4) << 35;
    pressure_var1 =
        ((pressure_var1 * pressure_var1 * (int64_t)dig_p3) >> 8) + ((pressure_var1 * (int64_t)dig_p2) << 12);
    pressure_var1 = (((((int64_t)1 << 47) + pressure_var1) * (int64_t)dig_p1) >> 33);

    if (pressure_var1 == 0)
    {
        return false;
    }

    int64_t compensated_pressure = 1048576 - pressure_raw;
    compensated_pressure = (((compensated_pressure << 31) - pressure_var2) * 3125) / pressure_var1;
    pressure_var1 = ((int64_t)dig_p9 * (compensated_pressure >> 13) * (compensated_pressure >> 13)) >> 25;
    pressure_var2 = ((int64_t)dig_p8 * compensated_pressure) >> 19;
    compensated_pressure = ((compensated_pressure + pressure_var1 + pressure_var2) >> 8) + ((int64_t)dig_p7 << 4);
    uint32_t pressure_pa = (uint32_t)(compensated_pressure >> 8);
    *pressure = (float)pressure_pa / 100.0f;

    return true;
}