# Envonitor

Environmental monitoring firmware for STM32F103C8T6 written in C using CMSIS and direct peripheral register access.

The project monitors temperature, humidity and atmospheric pressure, displays measurements on an OLED and provides configurable alarms. It was built to practice low-level embedded development.

## Features

- AHT20 temperature and humidity measurement
- BMP280 temperature and pressure measurement
- SSD1306 measurement display
- Potentiometer-based threshold control
- UART command interface

## Hardware

- STM32F103C8T6
- ST-Link V2
- AHT20 + BMP280 module
- SSD1306 128x64 I2C OLED
- CP2102 USB-UART adapter
- 20 kΩ potentiometer

## Connections

- USART1: PA9 TX, PA10 RX, 115200 8N1
- I2C1: PB6 SCL, PB7 SDA, 100 kHz
- ADC1: PA0

## UART commands

Format:

```text
<sensor> <operator> <threshold>
<sensor> off
```

Sensors:

`t` — temperature (-20..60 °C)  
`h` — humidity (0..100 %)  
`p` — pressure (900..1100 hPa)

Operators:

`>` — trigger above threshold  
`<` — trigger below threshold

Examples:

```text
t > 30
h < 40
p < 980
h < pot
t off
```

`pot` uses the potentiometer as the threshold.

## Build

Requirements:

- `arm-none-eabi-gcc`
- `GNU Make`
- `openocd`

Build:

```bash
make
```

Flash:

```bash
make flash
```

Clean:

```bash
make clean
```