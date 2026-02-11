/*
 * Sensor Interface
 * Handles heart rate and temperature sensor operations
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

// Sensor initialization
void heart_rate_sensor_init(void);
void temperature_sensor_init(void);

// Sensor reading functions
uint8_t read_heart_rate(void);      // Returns BPM (0-255)
float read_temperature(void);       // Returns temperature in Celsius

// Sensor calibration
void calibrate_heart_rate_sensor(void);
void calibrate_temperature_sensor(void);

// Sensor status
bool is_heart_rate_sensor_ready(void);
bool is_temperature_sensor_ready(void);

#endif // SENSORS_H