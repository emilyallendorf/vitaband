/*
 * Sensor Interface
 * Handles heart rate and temperature sensor operations
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include "tmp117.h"
#include "sht3x-dis.h"
#include "max86140.h"
#include <stdint.h>


typedef enum {
    BODY,    
    AMBIENT
} temp_sensor_type_t;

typedef struct {
    bool body;
    bool ambient;
} temp_sensor_states_t;

// Sensor initialization
void heart_rate_sensor_init(void);
void temperature_sensor_init(temp_sensor_type_t);

// Sensor reading functions
uint8_t read_heart_rate(void); // Returns BPM (0-255)
float read_temperature(temp_sensor_type_t); // Returns temperature in Celsius

// Sensor calibration
void calibrate_heart_rate_sensor(void);
void calibrate_temperature_sensor(temp_sensor_type_t sensor);

// Sensor status
bool is_hr_sensor_ready(void);
bool are_temp_sensors_ready(void);

#endif // SENSORS_H