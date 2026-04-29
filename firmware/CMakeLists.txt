# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/dts)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vitaband_firmware)

# Source logic based on your configs
if(CONFIG_VITABAND_BLINKY_BRINGUP)
  target_sources(app PRIVATE src/main_blinky.c)
elseif(CONFIG_VITABAND_MAX86140_HR_DEMO)
  target_sources(app PRIVATE src/main_max86140_hr.c drivers/src/max86140.c)
elseif(CONFIG_VITABAND_BLE_HR_BODY_TEST)
  target_sources(app PRIVATE src/main_ble_hr_body.c src/state_manager.c drivers/src/max86140.c drivers/src/tmp117.c)
else()
  # Default full application
  target_sources(app PRIVATE
    src/main.c src/power.c src/sensors.c src/haptics.c src/mock_sensors.c
    src/state_manager.c src/test.c drivers/src/max86140.c drivers/src/tmp117.c
    drivers/src/sht3x-dis.c drivers/src/alarm.c drivers/src/buzzer.c 
  )
endif()

if(CONFIG_BT)
  target_sources(app PRIVATE src/ble.c)
endif()

target_include_directories(app PUBLIC inc drivers/inc)