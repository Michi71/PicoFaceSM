# PicoFaceSM -- ARP Solina String Ensemble auf dem RP2350.
# Geruest uebernommen von PicoFaceRD; ein einziges Firmware-Ziel: picofacesm.
#
# Die Klangerzeugung liegt unter src/solina + include/solina und ist
# hardwareunabhaengig -- dieselben Quellen uebersetzt der Host-Test unter
# test/ mit -DSOLINA_HOST_BUILD.

cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_EXTRAS_PATH "../lib/pico-extras")

set(PICO_PLATFORM "rp2350-arm-s")
set(PICO_BOARD "sparkfun_promicro_rp2350")

set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/pico-sdk")

# Pico SDK einbinden (muss vor project() stehen)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -Wl,--print-memory-usage")
SET(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--print-memory-usage")

project(picofacesm C CXX ASM)
enable_language(CXX)

pico_sdk_init()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR} ".")

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

add_executable(picofacesm
    src/usb_descriptors.c
    src/sm_main.cpp
    src/pico_hw.cpp
    src/midi_input_usb.cpp
    src/get_serial.c
    src/SM_Midi.cpp
    src/SM_Controller.cpp
    src/SM_Display.cpp
    src/SM_Synth_Bridge.cpp
    src/veeprom.cpp
    # --- Klangerzeugung (identisch mit dem Host-Test) ---
    src/solina/solina.cpp
    src/solina/solina_keyboard.cpp
    src/solina/solina_registers.cpp
    src/solina/solina_ensemble.cpp
)

target_link_libraries(picofacesm PRIVATE
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

target_compile_definitions(picofacesm PRIVATE
    RD_CLOCK_504=1        # 480 MHz, gleicher Takt wie das Master-Projekt
    USE_AUDIO_I2S=1
    PICO_USE_SW_SPIN_LOCKS=1
    PICO_AUDIO_I2S_DATA_PIN=26
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=27
    TARGET_RP2350=1
)

pico_set_program_name(picofacesm "PicoFaceSM")
pico_set_program_version(picofacesm "0.1")
pico_enable_stdio_uart(picofacesm 1)
pico_enable_stdio_usb(picofacesm 0)
pico_add_extra_outputs(picofacesm)
pico_set_boot_stage2(picofacesm slower_boot2)

target_link_options(picofacesm PRIVATE
    -Xlinker --print-memory-usage
)
