cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pico SDK discovery. The checked-in import helper makes this directory
# independently configurable with: cmake -S rp2040 -B build-rp2040. The
# guard lets the repository-root compatibility entry point perform the same
# required pre-project initialization before including this file.
if(NOT RP2040_PROJECT_INITIALIZED)
    if(WIN32)
        set(USERHOME $ENV{USERPROFILE})
    else()
        set(USERHOME $ENV{HOME})
    endif()
    set(sdkVersion 2.2.0)
    set(toolchainVersion 14_2_Rel1)
    set(picotoolVersion 2.2.0)
    set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
    if(EXISTS ${picoVscode})
        include(${picoVscode})
    endif()

    set(PICO_BOARD pico CACHE STRING "Board type")
    include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)

    project(redashboard C CXX ASM)
    pico_sdk_init()
endif()

set(REDASHBOARD_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/board_io.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/buttons.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/code_history.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/i2c_bus.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/oled_display.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/post_uart.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/sensors.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/sio_monitor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/src/p80_post_codes.c
)

function(configure_redashboard_target TARGET_NAME BINARY_TYPE)
    add_executable(${TARGET_NAME} ${REDASHBOARD_SOURCES})

    pico_generate_pio_header(${TARGET_NAME}
            ${CMAKE_CURRENT_LIST_DIR}/pio/post_uart_rx.pio)

    if("${BINARY_TYPE}" STREQUAL "no_flash")
        pico_set_binary_type(${TARGET_NAME} no_flash)
    endif()

    pico_set_program_name(${TARGET_NAME} "${TARGET_NAME}")
    pico_set_program_version(${TARGET_NAME} "0.1")

    pico_enable_stdio_uart(${TARGET_NAME} 0)
    pico_enable_stdio_usb(${TARGET_NAME} 1)

    target_link_libraries(${TARGET_NAME}
            pico_stdlib
            hardware_dma
            hardware_i2c
            hardware_pio
    )

    target_include_directories(${TARGET_NAME} PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/include
    )

    pico_add_extra_outputs(${TARGET_NAME})
endfunction()

# Normal flash/XIP target: P80-only.
configure_redashboard_target(redashboard flash)

# Flash/XIP variant with I2C P81 and 1.5-Mbaud OUT16 decoding.
configure_redashboard_target(redashboard_p81_16bit flash)
target_compile_definitions(redashboard_p81_16bit PRIVATE
        POSTCODE_P81_DECODE_ENABLED=1U)

# Normal P80-only RAM target.
configure_redashboard_target(redashboard_ram no_flash)
