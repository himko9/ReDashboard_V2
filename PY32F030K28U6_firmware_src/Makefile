TARGET := ez_dashboard_py32f030k28
BUILD_DIR := build

PREFIX ?= arm-none-eabi-
CC := $(PREFIX)gcc
AS := $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
.SHELLFLAGS := /C
make-dir = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
remove-build = if exist "$(subst /,\,$(BUILD_DIR))" rmdir /S /Q "$(subst /,\,$(BUILD_DIR))"
else
make-dir = mkdir -p "$(1)"
remove-build = rm -rf "$(BUILD_DIR)"
endif

PROJECT_SOURCES := \
	src/main.c \
	src/app/p80_post_codes.c \
	src/app/buttons.c \
	src/app/code_history.c \
	src/app/uart_parser.c \
	src/bsp/board.c \
	src/bsp/py32f0xx_it.c \
	src/drivers/sio_i2c_hw.c \
	src/drivers/soft_i2c.c \
	src/drivers/ssd1315.c \
	src/drivers/uart_fast_rx.c \
	src/drivers/uart_rx.c

VENDOR_SOURCES := \
	vendor/openpuya/Templates/PY32F030xx/system_py32f0xx.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_cortex.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_dma.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_gpio.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_i2c.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_pwr.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc_ex.c \
	vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_uart.c

ASM_SOURCES := vendor/openpuya/Templates/PY32F030xx/startup_py32f030xx.s

INCLUDES := \
	-I. \
	-Iinclude \
	-Ivendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Inc \
	-Ivendor/openpuya/Drivers/CMSIS/Device/PY32F0xx/Include \
	-Ivendor/openpuya/Drivers/CMSIS/Include

DEFINES := -DUSE_HAL_DRIVER -DPY32F030x8
ARCH_FLAGS := -mcpu=cortex-m0plus -mthumb
COMMON_FLAGS := $(ARCH_FLAGS) $(DEFINES) $(INCLUDES) -ffunction-sections -fdata-sections
CFLAGS := $(COMMON_FLAGS) -std=c11 -Os -g3 -Wall -Wextra -Wshadow \
	-fno-common -MMD -MP
ASFLAGS := $(COMMON_FLAGS) -Os -g3
LDFLAGS := $(ARCH_FLAGS) -specs=nano.specs -specs=nosys.specs \
	-Tlinker/py32f030x8.ld -Wl,--gc-sections \
	-Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref
LDLIBS := -lc -lm -lnosys

C_OBJECTS := $(addprefix $(BUILD_DIR)/,$(PROJECT_SOURCES:.c=.o) $(VENDOR_SOURCES:.c=.o))
ASM_OBJECTS := $(addprefix $(BUILD_DIR)/,$(ASM_SOURCES:.s=.o))
OBJECTS := $(C_OBJECTS) $(ASM_OBJECTS)
DEPS := $(C_OBJECTS:.o=.d)

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) linker/py32f030x8.ld
	@$(call make-dir,$(@D))
	$(CC) $(OBJECTS) $(LDFLAGS) $(LDLIBS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(OBJCOPY) -O binary -S $< $@

$(BUILD_DIR)/%.o: %.c
	@$(call make-dir,$(@D))
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s
	@$(call make-dir,$(@D))
	$(AS) $(ASFLAGS) -c $< -o $@

clean:
	@$(remove-build)

-include $(DEPS)
