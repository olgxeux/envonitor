#ifndef I2C1_H
#define I2C1_H

#include <stdbool.h>
#include <stdint.h>

void i2c1_init(void);

bool i2c1_write(uint8_t address, const uint8_t *data, uint32_t length);

bool i2c1_read(uint8_t address, uint8_t *data, uint32_t length);

bool i2c1_write_read(
    uint8_t address,
    const uint8_t *write_data,
    uint32_t write_length,
    uint8_t *read_data,
    uint32_t read_length
);

bool i2c1_probe(uint8_t address);

#endif
