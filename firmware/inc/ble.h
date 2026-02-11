/*
 * Bluetooth Low Energy Interface
 * Handles BLE communication with phone app
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdint.h>
#include <stdbool.h>

// Example
// // BLE Service and Characteristic UUIDs
// // Health Thermometer Service (standard)
// #define HEALTH_TEMP_SERVICE_UUID    0x1809
// #define TEMPERATURE_CHAR_UUID       0x2A1C

// // Heart Rate Service (standard)
// #define HEART_RATE_SERVICE_UUID     0x180D
// #define HEART_RATE_CHAR_UUID        0x2A37

// // Custom Service for combined data
// #define CUSTOM_WEARABLE_SERVICE_UUID  "12345678-1234-1234-1234-123456789abc"

// Data structure for BLE transmission
typedef struct {
    uint8_t heart_rate;
    float temperature;
    uint32_t timestamp;
} sensor_data_t;

// BLE initialization and control
void ble_init(void);
void ble_start_advertising(void);
void ble_stop_advertising(void);
bool ble_is_connected(void);

// Data transmission
void send_ble_data(const sensor_data_t *data);
void send_heart_rate(uint8_t hr);
void send_temperature(float temp);

// Command processing
void process_ble_commands(void);

// Connection callbacks (implement in your application)
void on_ble_connected(void);
void on_ble_disconnected(void);

#endif // BLUETOOTH_H
