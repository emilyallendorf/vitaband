# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vitaband_firmware)

if(CONFIG_VITABAND_BLINKY_BRINGUP)
  set(VITABAND_MAIN_SRC src/main_blinky.c)
else()
  set(VITABAND_MAIN_SRC src/main.c)
endif()

target_sources(app PRIVATE
  ${VITABAND_MAIN_SRC}
  src/ble.c
  src/power.c
  src/sensors.c
  src/haptics.c
  src/mock_sensors.c
  src/state_manager.c
  src/test.c
  drivers/src/tmp117.c
  drivers/src/max86140.c
  drivers/src/sht3x-dis.c
  drivers/src/alarm.c
  drivers/src/buzzer.c
)

if(CONFIG_SHELL)
  target_sources(app PRIVATE src/sensor_hw_shell.c)
endif()

target_include_directories(app PUBLIC
  inc
  drivers/inc
)

zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)
