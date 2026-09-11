@echo off
setlocal
setlocal EnableExtensions EnableDelayedExpansion
where arm-none-eabi-gcc >nul 2>&1
if /I "%~1"=="clean" (
    if exist build rmdir /S /Q build
    exit /b 0
)

for %%T in (arm-none-eabi-gcc arm-none-eabi-objcopy arm-none-eabi-size) do (
    where %%T >nul 2>&1
    if errorlevel 1 (
        echo Error: %%T was not found in PATH.
        exit /b 1
    )
)

set "TARGET=ez_dashboard_py32f030k28"
set "BUILD_DIR=build"
set "COMMON_FLAGS=-mcpu=cortex-m0plus -mthumb -DUSE_HAL_DRIVER -DPY32F030x8 -I. -Iinclude -Ivendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Inc -Ivendor/openpuya/Drivers/CMSIS/Device/PY32F0xx/Include -Ivendor/openpuya/Drivers/CMSIS/Include -ffunction-sections -fdata-sections"
set "CFLAGS=%COMMON_FLAGS% -std=c11 -Os -g3 -Wall -Wextra -Wshadow -fno-common"
set "LDFLAGS=-mcpu=cortex-m0plus -mthumb -specs=nano.specs -specs=nosys.specs -Tlinker/py32f030x8.ld -Wl,--gc-sections -Wl,-Map=%BUILD_DIR%/%TARGET%.map,--cref"

if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
mkdir "%BUILD_DIR%"

set "OBJECTS="
for %%F in (
    src/main.c
    src/app/p80_post_codes.c
    src/app/buttons.c
    src/app/code_history.c
    src/app/uart_parser.c
    src/bsp/board.c
    src/bsp/py32f0xx_it.c
    src/drivers/sio_i2c_hw.c
    src/drivers/soft_i2c.c
    src/drivers/ssd1315.c
    src/drivers/uart_fast_rx.c
    src/drivers/uart_rx.c
    vendor/openpuya/Templates/PY32F030xx/system_py32f0xx.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_cortex.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_dma.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_gpio.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_i2c.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_pwr.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_rcc_ex.c
    vendor/openpuya/Drivers/PY32F0xx_HAL_Driver/Src/py32f0xx_hal_uart.c
) do (
    echo Compiling %%F
    arm-none-eabi-gcc %CFLAGS% -c "%%F" -o "%BUILD_DIR%/%%~nF.o" || exit /b 1
    set "OBJECTS=!OBJECTS! %BUILD_DIR%/%%~nF.o"
)

echo Assembling startup_py32f030xx.s
arm-none-eabi-gcc -x assembler-with-cpp %COMMON_FLAGS% -Os -g3 -c "vendor/openpuya/Templates/PY32F030xx/startup_py32f030xx.s" -o "%BUILD_DIR%/startup_py32f030xx.o" || exit /b 1
set "OBJECTS=!OBJECTS! %BUILD_DIR%/startup_py32f030xx.o"

echo Linking %TARGET%.elf
arm-none-eabi-gcc !OBJECTS! %LDFLAGS% -lc -lm -lnosys -o "%BUILD_DIR%/%TARGET%.elf" || exit /b 1
arm-none-eabi-objcopy -O ihex "%BUILD_DIR%/%TARGET%.elf" "%BUILD_DIR%/%TARGET%.hex" || exit /b 1
arm-none-eabi-objcopy -O binary -S "%BUILD_DIR%/%TARGET%.elf" "%BUILD_DIR%/%TARGET%.bin" || exit /b 1
arm-none-eabi-size "%BUILD_DIR%/%TARGET%.elf"
if errorlevel 1 (
    exit /b 1
)

echo Build complete: %BUILD_DIR%\%TARGET%.elf, .hex, .bin
exit /b 0
exit /b %errorlevel%