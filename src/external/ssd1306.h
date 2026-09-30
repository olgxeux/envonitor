#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>

bool ssd1306_init(void);

void ssd1306_clear(void);

void ssd1306_set_pixel(uint8_t x, uint8_t y, bool enabled);

void ssd1306_draw_string(uint8_t x, uint8_t y, const char *string);

bool ssd1306_update(void);

#endif