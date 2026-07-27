# PicoFaceRD -- Roland MKS-20/MK-80 clone on RP2350
# Single firmware target: picofacerd2 (descriptor-driven RdNewEngine).
# The retired DX target and the v1 MAME-emulation target live in deleted/;
# the reference emulator sources remain in src/rd_engine for the host
# toolchain (tools/rd_extract).

cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_EXTRAS_PATH "../lib/pico-extras")

set(PICO_PLATFORM "rp2350-arm-s")
set(PICO_BOARD "sparkfun_promicro_rp2350")

set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -Wl,--print-memory-usage")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--print-memory-usage")

project(picofacerd2 C CXX ASM)
enable_language(CXX)

pico_sdk_init()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/effects ${CMAKE_CURRENT_BINARY_DIR} ".")

add_subdirectory(lib/audio)
add_subdirectory(lib/encoder)
add_subdirectory(lib/u8g2)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -ffast-math
        )
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=3)

# --- PicoFaceRD v2: descriptor-driven RdNewEngine target ---
add_executable(picofacerd2
    src/usb_descriptors.c
    src/rd_main.cpp
    src/pico_hw.cpp
    src/midi_input_usb.cpp
    src/get_serial.c
    src/RD_Midi.cpp
    src/RD_Controller.cpp
    src/RD_Display.cpp
    src/veeprom.cpp
    src/rd_effects.cpp
    src/RD_Synth_Bridge_v2.cpp
    src/rd_engine/rd_new_engine.cpp
    src/rd_engine/rd_packs_data.cpp
    src/rd_engine/program_tables.cpp
    src/rd_engine/rd_samples_pk4_a.cpp
    src/rd_engine/rd_samples_pk4_b.cpp
    src/rd_engine/rd_samples_pk4_m.cpp
)

target_include_directories(picofacerd2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rd_engine
)

target_link_libraries(picofacerd2 PRIVATE
    pico_stdlib
    hardware_adc
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_interp
    hardware_timer
    hardware_watchdog
    hardware_clocks
    hardware_sync_spin_lock
    hardware_sync
    hardware_flash
    pico_unique_id
    pico_bootsel_via_double_reset
    pico_util_buffer
    pico_multicore
    pico_stdio_uart
    tinyusb_device
    tinyusb_board
    Audio
    RotaryEncoder
    u8g2
)

target_compile_definitions(picofacerd2 PRIVATE
    RD_ENGINE_V2=1
    RD_CLOCK_504=1
    USE_AUDIO_I2S=1
    PICO_USE_SW_SPIN_LOCKS=1
    PICO_AUDIO_I2S_DATA_PIN=26
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=27
    TARGET_RP2350=1
)

pico_set_program_name(picofacerd2 "PicoFaceRD2")
pico_set_program_version(picofacerd2 "0.1")
pico_enable_stdio_uart(picofacerd2 1)
pico_enable_stdio_usb(picofacerd2 0)
pico_add_extra_outputs(picofacerd2)
pico_set_boot_stage2(picofacerd2 slower_boot2)

target_link_options(picofacerd2 PRIVATE
    -Xlinker --print-memory-usage
)
