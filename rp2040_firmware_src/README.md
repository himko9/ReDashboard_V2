# ReDashboard V2 - RP2040

This directory is a standalone Raspberry Pi Pico SDK project. It contains its
own firmware sources, configuration headers, POST-code table, PIO program,
Pico SDK import helper, and CMake project.

The user-editable configuration files are under `include/`:

- `app_config.h`: polling, display, button, sensor timing, and the percentage
  change required to wake the OLED.
- `display_config.h`: enabled pages, startup page, P81 decoding, and OLED
  power-saving/top-bar behavior.
- `sio_adc_config.h`: voltage/temperature channels, labels, and formulas.
- `sio_fan_config.h`: tachometer channels, labels, and formulas.
- `board_config.h`: RP2040 pins, buses, UARTs, buttons, and OLED rotation.

Configure and build from this directory with Pico SDK 2.2 or later:

```sh
cmake -S . -B build
cmake --build build --parallel
```

Targets:

- `redashboard`: normal P80-only flash build.
- `redashboard_p81_16bit`: P81/OUT16 flash build.
- `redashboard_ram`: normal P80-only RAM build.

By default, a valid sensor change greater than 10 percent wakes a dimmed or
powered-off OLED. The static source/role/lock top bar hides after 30 seconds
and reappears on activity. Configure these with
`SENSOR_WAKE_CHANGE_PERCENT` in `app_config.h` and
`DISPLAY_TOP_BAR_TIMEOUT_SECONDS` in `display_config.h`; zero disables the
corresponding feature.

UART mode keeps a lightweight P80-only JDASH probe active. A stable value is
used only as a baseline; a subsequent valid nonzero P80 transition switches
the display back to JDASH automatically. Configure its background interval
with `JDASH_DETECT_POLL_INTERVAL_MS` in `app_config.h`.
