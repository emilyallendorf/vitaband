# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vitaband_firmware)

target_sources(app PRIVATE
    src/main.c
    src/ble.c
    src/power.c
    src/sensors.c
    src/haptics.c
    src/mock_sensors.c
    src/state_manager.c
    src/test.c
    drivers/src/tmp117.c
    # drivers/src/max86140.c
    drivers/src/sht3x-dis.c
    drivers/src/alarm.c
    drivers/src/buzzer.c
)

target_include_directories(app PUBLIC 
    inc 
    drivers/inc
)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE
  ${app_sources}
  )

zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)
