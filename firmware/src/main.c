/*
 * Wearable Device Firmware
 * Continuously monitors heart rate and temperature sensors
 * Communicates via Bluetooth Low Energy (BLE)
 */

#include <stdint.h>
#include <stdbool.h>
#include "sensors.h"
#include "ble.h"
#include "power.h"

// Measurement intervals (in milliseconds)
#define HEART_RATE_INTERVAL_MS    1000  // 1 second
#define TEMPERATURE_INTERVAL_MS   1000  // 1 second
#define BLE_UPDATE_INTERVAL_MS    1000  // 1 second

typedef struct {
    uint8_t heart_rate;      // beats per minute
    float temperature;       // celsius
    uint32_t timestamp;      // milliseconds since boot
} sensor_data_t;

// Global variables
static sensor_data_t current_data = {0};
static volatile bool sensor_data_ready = false;

// Function prototypes
void system_init(void);
void process_sensors(void);
void update_bluetooth(void);
void enter_low_power_mode(void); //?

int main(void) {
    system_init();

    uint32_t last_heart_rate_measurement = 0;
    uint32_t last_temp_measurement = 0;
    uint32_t last_ble_update = 0;

    //Main loop
    while (1) {
        uint32_t current_time = get_system_time_ms(); //?

        // Measure heart rate
        if ((current_time - last_heart_rate_measurement) >= HEART_RATE_INTERVAL_MS) {
            current_data.heart_rate = read_heart_rate(); //sensor.h
            last_heart_rate_measurement = current_time;
            sensor_data_ready = true;
        }
        
        // Measure temperature
        if ((current_time - last_temp_measurement) >= TEMPERATURE_INTERVAL_MS) {
            current_data.temperature = read_temperature(); //sensor.h
            last_temp_measurement = current_time;
            sensor_data_ready = true;
        }
        
        // Update Bluetooth if new data is available
        if (sensor_data_ready && 
            (current_time - last_ble_update) >= BLE_UPDATE_INTERVAL_MS) {
            current_data.timestamp = current_time;
            send_ble_data(&current_data); //?
            last_ble_update = current_time;
            sensor_data_ready = false;
        }

        // Process any incoming Bluetooth commands
        process_ble_commands(); //?
        
        // Enter low power mode until next event
        enter_low_power_mode(); //?
    }
    return 0;
}

// Function Implementations
void system_init(void) {
    // Initialize system clock
    init_system_clock();
    
    // Initialize sensors
    heart_rate_sensor_init();
    temperature_sensor_init();
    
    // Initialize Bluetooth
    ble_init();
    ble_start_advertising();
    
    // Initialize power management
    power_management_init();
}