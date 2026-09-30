TARGET := firmware
BUILD_DIR := build

CC      := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE    := arm-none-eabi-size
OPENOCD := openocd


CPU_FLAGS := \
	-mcpu=cortex-m3 \
	-mthumb

DEFINES := \
	-DSTM32F103xB

INCLUDES := \
	-Icmsis/core \
	-Icmsis/device \
	-Isrc/app \
	-Isrc/common \
	-Isrc/external \
	-Isrc/mcu


CFLAGS := \
	$(CPU_FLAGS) \
	-std=c11 \
	-Og \
	-g3 \
	-Wall \
	-Wextra \
	-Werror \
	-ffunction-sections \
	-fdata-sections \
	-MMD \
	-MP

CPPFLAGS := \
	$(DEFINES) \
	$(INCLUDES)


LDSCRIPT := linker/STM32F103XB_FLASH.ld

LDFLAGS := \
	$(CPU_FLAGS) \
	-T$(LDSCRIPT) \
	-Wl,--gc-sections \
	-Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
	-Wl,--print-memory-usage


C_SOURCES := \
	src/main.c \
	src/app/app.c \
	src/common/format.c \
	src/common/ring_buffer.c \
	src/external/aht20.c \
	src/external/bmp280.c \
	src/external/ssd1306.c \
	src/mcu/adc1.c \
	src/mcu/clock.c \
	src/mcu/i2c1.c \
	src/mcu/system_stm32f1xx.c \
	src/mcu/time.c \
	src/mcu/usart1.c

ASM_SOURCES := \
	startup/startup_stm32f103xb.s


C_OBJECTS := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o)
ASM_OBJECTS := $(ASM_SOURCES:%.s=$(BUILD_DIR)/%.o)

OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)
DEPS := $(C_OBJECTS:.o=.d)

ELF := $(BUILD_DIR)/$(TARGET).elf
BIN := $(BUILD_DIR)/$(TARGET).bin


.PHONY: all clean flash

all: $(ELF) $(BIN)


$(ELF): $(OBJECTS) $(LDSCRIPT)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@


$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(CC) $(CPU_FLAGS) -g3 -c $< -o $@


$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@


flash: $(ELF)
	$(OPENOCD) \
		-f interface/stlink.cfg \
		-f target/stm32f1x.cfg \
		-c "program $< verify reset exit"


clean:
	rm -rf $(BUILD_DIR)


-include $(DEPS)