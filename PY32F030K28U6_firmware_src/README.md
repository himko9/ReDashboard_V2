# EZ Dashboard PY32F030K28 Firmware

Standalone release source for the PY32F030K28U6 target. The required Puya
CMSIS/HAL files, startup code, and linker script are included.

## Requirements

- Arm GNU Toolchain with `arm-none-eabi-gcc` in `PATH`
- GNU Make for the Makefile build

## Build

On Windows:

```bat
build.bat
```

The Windows script only requires the Arm GNU Toolchain; GNU Make is not needed.

On Linux or macOS:

```sh
make
```

To use a toolchain outside `PATH`, set its executable prefix:

```sh
make PREFIX=/opt/arm-gnu-toolchain/bin/arm-none-eabi-
```

Build outputs are written to `build/`:

- `ez_dashboard_py32f030k28.elf`
- `ez_dashboard_py32f030k28.hex`
- `ez_dashboard_py32f030k28.bin`

The raw `.bin` image starts at flash address `0x08000000`. Use `make clean`
or `build.bat clean` to remove generated files.

Application and board settings are in the headers under `include/`.