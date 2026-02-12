/*
 * Configuration File
 * Customize all firmware parameters here
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// MEASUREMENT INTERVALS
// ============================================================================
#define HEART_RATE_INTERVAL_MS      1000    // 1 Hz - adjust as needed
#define TEMPERATURE_INTERVAL_MS     5000    // 0.2 Hz
#define BLE_UPDATE_INTERVAL_MS      1000    // How often to send BLE updates

// ============================================================================
// SENSOR CONFIGURATION
// ============================================================================

// Heart Rate Sensor
#define HR_SENSOR_TYPE              MAX30102
#define HR_SENSOR_I2C_ADDR          0x57
#define HR_LED_CURRENT_MA           50      // LED current (mA)
#define HR_SAMPLE_RATE_HZ           100     // Sampling rate
#define HR_PULSE_WIDTH_US           411     // LED pulse width

// Temperature Sensor
#define TEMP_SENSOR_TYPE            TMP117
#define TEMP_SENSOR_I2C_ADDR        0x48
#define TEMP_SENSOR_RESOLUTION      0.0078125  // Resolution in °C

// ============================================================================
// BLUETOOTH CONFIGURATION
// ============================================================================
#define BLE_DEVICE_NAME             "Wearable"
#define BLE_ADV_INTERVAL_MS         100     // Advertising interval
#define BLE_CONN_INTERVAL_MS        50      // Min connection interval
#define BLE_CONN_TIMEOUT_MS         4000    // Connection timeout

// Service UUIDs (standard services)
#define USE_STANDARD_HR_SERVICE     1       // Use standard Heart Rate Service
#define USE_STANDARD_TEMP_SERVICE   1       // Use standard Temperature Service
#define USE_CUSTOM_SERVICE          1       // Use custom combined service

// ============================================================================
// POWER MANAGEMENT
// ============================================================================
#define ENABLE_LOW_POWER_MODE       1       // Enable low power between measurements
#define BATTERY_LOW_THRESHOLD       10      // Battery low warning (%)
#define BATTERY_CRITICAL_THRESHOLD  5       // Battery critical (%)

// Power saving features
#define REDUCE_BLE_TX_POWER         1       // Reduce TX power to save battery
#define BLE_TX_POWER_DBM            0       // TX power in dBm (-40 to +4)
#define ENABLE_MOTION_WAKE          1       // Wake on motion (if accelerometer available)

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================
#define SYSTEM_CLOCK_HZ             64000000  // System clock frequency
#define SYSTICK_FREQ_HZ             1000      // SysTick frequency (1ms ticks)

// Debug configuration
#define DEBUG_ENABLED               0         // Enable debug output
#define DEBUG_UART_BAUDRATE         115200

// ============================================================================
// SENSOR LIMITS AND VALIDATION
// ============================================================================
#define HR_MIN_BPM                  30        // Minimum valid heart rate
#define HR_MAX_BPM                  220       // Maximum valid heart rate
#define TEMP_MIN_C                  30.0      // Minimum valid temperature
#define TEMP_MAX_C                  45.0      // Maximum valid temperature

// ============================================================================
// DATA BUFFERING
// ============================================================================
#define ENABLE_DATA_LOGGING         0         // Log data to flash
#define DATA_BUFFER_SIZE            100       // Number of measurements to buffer
#define FLASH_LOG_INTERVAL_SEC      3600      // Write to flash every hour

// ============================================================================
// FEATURES
// ============================================================================
#define ENABLE_BATTERY_SERVICE      1         // BLE Battery Service
#define ENABLE_DEVICE_INFO_SERVICE  1         // BLE Device Information Service
#define ENABLE_OTA_UPDATE           0         // Over-the-air firmware update
#define ENABLE_RTC                  0         // Real-time clock for timestamps

#endif // CONFIG_H