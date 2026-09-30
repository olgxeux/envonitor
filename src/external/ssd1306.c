#include "ssd1306.h"

#include "i2c1.h"
#include "time.h"

#include <stdint.h>

#define SSD1306_I2C_ADDRESS 0x3CU

#define SSD1306_WIDTH 128U
#define SSD1306_HEIGHT 64U
#define SSD1306_PAGE_HEIGHT 8U
#define SSD1306_PAGE_COUNT (SSD1306_HEIGHT / SSD1306_PAGE_HEIGHT)
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_PAGE_COUNT)

#define SSD1306_CONTROL_COMMAND 0x00U
#define SSD1306_CONTROL_DATA 0x40U

#define SSD1306_COMMAND_DISPLAY_OFF 0xAEU
#define SSD1306_COMMAND_DISPLAY_ON 0xAFU
#define SSD1306_COMMAND_CLOCK 0xD5U
#define SSD1306_CLOCK_DEFAULT 0x80U
#define SSD1306_COMMAND_MULTIPLEX 0xA8U
#define SSD1306_MULTIPLEX_64 0x3FU
#define SSD1306_COMMAND_DISPLAY_OFFSET 0xD3U
#define SSD1306_DISPLAY_OFFSET_NONE 0x00U
#define SSD1306_COMMAND_START_LINE 0x40U
#define SSD1306_COMMAND_CHARGE_PUMP 0x8DU
#define SSD1306_CHARGE_PUMP_ENABLE 0x14U
#define SSD1306_COMMAND_MEMORY_MODE 0x20U
#define SSD1306_MEMORY_MODE_PAGE 0x02U
#define SSD1306_COMMAND_SEGMENT_REMAP 0xA1U
#define SSD1306_COMMAND_COM_SCAN_REVERSE 0xC8U
#define SSD1306_COMMAND_COM_PINS 0xDAU
#define SSD1306_COM_PINS_128X64 0x12U
#define SSD1306_COMMAND_CONTRAST 0x81U
#define SSD1306_CONTRAST_DEFAULT 0xCFU
#define SSD1306_COMMAND_PRECHARGE 0xD9U
#define SSD1306_PRECHARGE_DEFAULT 0xF1U
#define SSD1306_COMMAND_VCOM_DESELECT 0xDBU
#define SSD1306_VCOM_DEFAULT 0x40U
#define SSD1306_COMMAND_DISPLAY_RAM 0xA4U
#define SSD1306_COMMAND_NORMAL_DISPLAY 0xA6U
#define SSD1306_COMMAND_PAGE_START 0xB0U
#define SSD1306_COMMAND_COLUMN_LOW 0x00U
#define SSD1306_COMMAND_COLUMN_HIGH 0x10U

#define SSD1306_POWER_ON_DELAY_MS 100U

typedef struct
{
    char character;
    uint8_t columns[5];
} Glyph;

static const Glyph font[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00}}, {'!', {0x00, 0x00, 0x5F, 0x00, 0x00}}, {'*', {0x14, 0x08, 0x3E, 0x08, 0x14}},
    {'-', {0x08, 0x08, 0x08, 0x08, 0x08}}, {'.', {0x00, 0x60, 0x60, 0x00, 0x00}}, {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
    {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}}, {'2', {0x42, 0x61, 0x51, 0x49, 0x46}}, {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
    {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}}, {'5', {0x27, 0x45, 0x45, 0x45, 0x39}}, {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
    {'7', {0x01, 0x71, 0x09, 0x05, 0x03}}, {'8', {0x36, 0x49, 0x49, 0x49, 0x36}}, {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
    {'<', {0x08, 0x14, 0x22, 0x41, 0x00}}, {'>', {0x00, 0x41, 0x22, 0x14, 0x08}}, {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
    {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}}, {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}}, {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
    {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}}, {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}}, {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
    {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}}, {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}}, {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
    {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}}, {'Y', {0x03, 0x04, 0x78, 0x04, 0x03}}
};

static uint8_t framebuffer[SSD1306_BUFFER_SIZE];

static const uint8_t *ssd1306_find_glyph(char character)
{
    uint32_t count = sizeof(font) / sizeof(font[0]);

    for (uint32_t i = 0U; i < count; ++i)
    {
        if (font[i].character == character)
        {
            return font[i].columns;
        }
    }

    return font[0].columns;
}

void ssd1306_clear(void)
{
    for (uint32_t i = 0U; i < SSD1306_BUFFER_SIZE; ++i)
    {
        framebuffer[i] = 0U;
    }
}

void ssd1306_set_pixel(uint8_t x, uint8_t y, bool enabled)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
    {
        return;
    }

    uint32_t page = y / SSD1306_PAGE_HEIGHT;

    uint32_t index = page * SSD1306_WIDTH + x;

    uint8_t bit = (uint8_t)(1U << (y % SSD1306_PAGE_HEIGHT));

    if (enabled)
    {
        framebuffer[index] |= bit;
    }
    else
    {
        framebuffer[index] &= (uint8_t)~bit;
    }
}

void ssd1306_draw_string(uint8_t x, uint8_t y, const char *string)
{
    if (string == 0)
    {
        return;
    }

    while (*string != '\0')
    {
        if (x + 5U > SSD1306_WIDTH)
        {
            return;
        }

        const uint8_t *glyph = ssd1306_find_glyph(*string);

        for (uint8_t column = 0U; column < 5U; ++column)
        {
            for (uint8_t row = 0U; row < 7U; ++row)
            {
                if (glyph[column] & (1U << row))
                {
                    ssd1306_set_pixel((uint8_t)(x + column), (uint8_t)(y + row), true);
                }
            }
        }
        
        x = (uint8_t)(x + 6U);
        ++string;
    }
}

bool ssd1306_update(void)
{
    for (uint8_t page = 0U; page < SSD1306_PAGE_COUNT; ++page)
    {
        uint8_t position_command[] = {
            SSD1306_CONTROL_COMMAND,
            (uint8_t)(SSD1306_COMMAND_PAGE_START + page),
            SSD1306_COMMAND_COLUMN_LOW,
            SSD1306_COMMAND_COLUMN_HIGH
        };

        if (!i2c1_write(SSD1306_I2C_ADDRESS, position_command, sizeof(position_command)))
        {
            return false;
        }

        uint8_t display_data[SSD1306_WIDTH + 1U];

        display_data[0] = SSD1306_CONTROL_DATA;

        for (uint32_t column = 0U; column < SSD1306_WIDTH; ++column)
        {
            display_data[column + 1U] = framebuffer[(uint32_t)page * SSD1306_WIDTH + column];
        }

        if (!i2c1_write(SSD1306_I2C_ADDRESS, display_data, sizeof(display_data)))
        {
            return false;
        }
    }

    return true;
}

bool ssd1306_init(void)
{
    delay_ms(SSD1306_POWER_ON_DELAY_MS);

    static const uint8_t init_commands[] = {
        SSD1306_CONTROL_COMMAND,        SSD1306_COMMAND_DISPLAY_OFF,   SSD1306_COMMAND_CLOCK,
        SSD1306_CLOCK_DEFAULT,          SSD1306_COMMAND_MULTIPLEX,     SSD1306_MULTIPLEX_64,
        SSD1306_COMMAND_DISPLAY_OFFSET, SSD1306_DISPLAY_OFFSET_NONE,   SSD1306_COMMAND_START_LINE,
        SSD1306_COMMAND_CHARGE_PUMP,    SSD1306_CHARGE_PUMP_ENABLE,    SSD1306_COMMAND_MEMORY_MODE,
        SSD1306_MEMORY_MODE_PAGE,       SSD1306_COMMAND_SEGMENT_REMAP, SSD1306_COMMAND_COM_SCAN_REVERSE,
        SSD1306_COMMAND_COM_PINS,       SSD1306_COM_PINS_128X64,       SSD1306_COMMAND_CONTRAST,
        SSD1306_CONTRAST_DEFAULT,       SSD1306_COMMAND_PRECHARGE,     SSD1306_PRECHARGE_DEFAULT,
        SSD1306_COMMAND_VCOM_DESELECT,  SSD1306_VCOM_DEFAULT,          SSD1306_COMMAND_DISPLAY_RAM,
        SSD1306_COMMAND_NORMAL_DISPLAY, SSD1306_COMMAND_DISPLAY_ON
    };

    if (!i2c1_write(SSD1306_I2C_ADDRESS, init_commands, sizeof(init_commands)))
    {
        return false;
    }

    ssd1306_clear();

    return ssd1306_update();
}