/*
 * Sensor Interface
 * Handles heart rate and temperature sensor operations
 */

#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stdint.h>
#include <config.h>
#include "tmp117.h"
#include "sht3x-dis.h"
#include "max86140.h"


typedef enum {
    BODY,    
    AMBIENT
} temp_sensor_type_t;

typedef struct {
    bool body;
    bool ambient;
} temp_sensor_states_t;

// Sensor initialization
 
/**
 * @brief Initialize the heart rate sensor (MAX86140).
 */
void heart_rate_sensor_init(void);
 
/**
 * @brief Initialize a temperature sensor.
 * @param sensor BODY (TMP117) or AMBIENT (SHT3x).
 */
void temperature_sensor_init(temp_sensor_type_t sensor);
 
// Readings 
 
/**
 * @brief Read heart rate in BPM.
 * @return BPM, or 0 if the sensor is not initialized.
 */
uint8_t read_heart_rate(void);
 
/**
 * @brief Read relative humidity in percent.
 * @return Humidity %, or 0.0 if the sensor is not initialized.
 */
float read_humidity(void);
 
/**
 * @brief Read temperature in °C.
 * @param sensor BODY or AMBIENT.
 * @return Temperature in °C, or -99.0 on error / not initialized.
 */
float read_temperature(temp_sensor_type_t sensor);
 
// Calibration 
 
/**
 * @brief Mark the heart rate sensor as calibrated.
 */
void calibrate_heart_rate_sensor(void);
 
/**
 * @brief Mark a temperature sensor as calibrated.
 * @param sensor BODY or AMBIENT.
 */
void calibrate_temperature_sensor(temp_sensor_type_t sensor);
 
// Readiness checks 
 
/**
 * @brief Returns true if the HR sensor is initialized AND calibrated.
 */
bool is_hr_sensor_ready(void);
 
/**
 * @brief Returns true if the given temp sensor is initialized AND calibrated.
 * @param sensor BODY or AMBIENT.
 */
bool is_temp_sensor_ready(temp_sensor_type_t sensor);

#endif // SENSORS_H