# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Lets editors (C/C++, clangd) resolve Zephyr headers after `west build`.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vitaband_firmware)

if(CONFIG_VITABAND_BLINKY_BRINGUP)
  message(STATUS "VitaBand: main = src/main_blinky.c (CONFIG_VITABAND_BLINKY_BRINGUP=y)")
  target_sources(app PRIVATE
    src/main_blinky.c)
elseif(CONFIG_VITABAND_PWM_I2C_PIN_SCOPE)
  message(STATUS "VitaBand: main = src/main_pwm_i2c_pins_scope.c (PWM on P0.26/P0.27)")
  target_sources(app PRIVATE
    src/main_pwm_i2c_pins_scope.c)
elseif(CONFIG_VITABAND_MAX86140_HR_DEMO)
  message(STATUS "VitaBand: main = src/main_max86140_hr.c (MAX86140 SPI HR)")
  target_sources(app PRIVATE
    src/main_max86140_hr.c
    drivers/src/max86140.c
  )
elseif(CONFIG_VITABAND_AMBIENT_PWM_DEMO)
  message(STATUS "VitaBand: main = src/main_ambient_pwm.c (ambient SHT3XD sensor API demo)")
  target_sources(app PRIVATE
    src/main_ambient_pwm.c
  )
elseif(CONFIG_VITABAND_SPARKFUN_TMP102_DK)
  message(STATUS "VitaBand: main = src/main_sparkfun_tmp102_dk.c (SparkFun TMP102 DK I2C)")
  target_sources(app PRIVATE
    src/main_sparkfun_tmp102_dk.c
  )
elseif(CONFIG_VITABAND_TMP117_I2C_DEMO)
  message(STATUS "VitaBand: main = src/main_tmp117_i2c.c (TMP117 I2C printk test)")
  target_sources(app PRIVATE
    src/main_tmp117_i2c.c
    drivers/src/tmp117.c
  )
elseif(CONFIG_VITABAND_BLE_BODY_TEMP_TEST)
  message(STATUS "VitaBand: main = src/main_ble_body_temp.c (BLE + TMP117 body temp)")
  target_sources(app PRIVATE
    src/main_ble_body_temp.c
    drivers/src/tmp117.c
  )
elseif(CONFIG_VITABAND_BLE_HR_BODY_TEST)
  message(STATUS "VitaBand: main = src/main_ble_hr_body.c (BLE + MAX86140 HR + TMP117)")
  target_sources(app PRIVATE
    src/main_ble_hr_body.c
    drivers/src/max86140.c
    drivers/src/tmp117.c
  )
elseif(CONFIG_VITABAND_BLE_TEST)
  message(STATUS "VitaBand: main = src/main_ble_test.c (standalone BLE test)")
  target_sources(app PRIVATE
    src/main_ble_test.c
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
