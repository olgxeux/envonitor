#ifndef USART1_H
#define USART1_H

#include <stdbool.h>
#include <stdint.h>

void usart1_init(void);

bool usart1_try_read_char(char *value);

bool usart1_try_queue_char(char value);

uint32_t usart1_queue_string(const char *string);

#endif
