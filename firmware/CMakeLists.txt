# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

option(VITABAND_BRINGUP_OVERLAY "Append bringup.overlay (disable &i2c0 and &spi1)" OFF)
if(VITABAND_BRINGUP_OVERLAY)
  list(APPEND EXTRA_DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/bringup.overlay")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vitaband_firmware)

if(CONFIG_VITABAND_BLINKY_BRINGUP)
  message(STATUS "VitaBand: main = src/main_blinky.c (CONFIG_VITABAND_BLINKY_BRINGUP=y)")
  target_sources(app PRIVATE
    src/main_blinky.c)
elseif(CONFIG_VITABAND_AMBIENT_PWM_DEMO)
  message(STATUS "VitaBand: main = src/main_ambient_pwm.c (ambient SHT3x printk demo)")
  target_sources(app PRIVATE
    src/main_ambient_pwm.c
    drivers/src/sht3x-dis.c
  )
elseif(CONFIG_VITABAND_TMP102_PWM_DEMO)
  message(STATUS "VitaBand: main = src/main_tmp102_pwm.c (CONFIG_VITABAND_TMP102_PWM_DEMO=y)")
  target_sources(app PRIVATE
    src/main_tmp102_pwm.c
    drivers/src/tmp102.c
  )
else()
  message(STATUS "VitaBand: main = src/main.c (full application)")
  target_sources(app PRIVATE
    src/main.c
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
endif()

if(CONFIG_BT)
  target_sources(app PRIVATE src/ble.c)
endif()

if(CONFIG_SHELL)
  target_sources(app PRIVATE src/sensor_hw_shell.c)
endif()

target_include_directories(app PUBLIC
  inc
  drivers/inc
)

if(CONFIG_BT)
  zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)
endif()
