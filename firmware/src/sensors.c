/*
 * Sensor Implementation
 */

#include "sensors.h"
#include <errno.h>
#include <stdbool.h>
#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(sensors, LOG_LEVEL_INF);

#define NO_HR 0
#define NO_TEMP -99.0f
#define NO_HUM 0.0f


// Internal state
static bool heart_rate_sensor_initialized = false;
temp_sensor_states_t temperature_sensors_initialized = {
    .body = false,
    .ambient = false
};
static bool heart_rate_sensor_calibrated = false;
temp_sensor_states_t temperature_sensors_calibrated = {
    .body = false,
    .ambient = false
};

void heart_rate_sensor_init(void) {
    // int ret = max86140_init();
    // if (ret == 0) heart_rate_sensor_initialized = true;
    return true;
}

void temperature_sensor_init(temp_sensor_type_t sensor) {
    int ret;
    switch(sensor) {
        case BODY:
            ret = tmp117_init();
            if (ret == 0) temperature_sensors_initialized.body = true;
            LOG_INF("Body temperature sensor initialized successfully.");
            return;
        case AMBIENT:
            ret = sht3xdis_init();
            if (ret == 0) temperature_sensors_initialized.ambient = true;
            LOG_INF("Ambient temperature sensor initialized successfully.");
            return;
        default: 
            LOG_ERR("Invalid sensor type provided: %d", sensor);
            ret = EINVAL;
            return;
    }
}

uint8_t read_heart_rate(void) {
    if (!temperature_sensors_initialized.ambient) return NO_HUM;
    // uint8_t heart_rate = max86140_read_heartrate();
    uint8_t heart_rate = 42;
    return heart_rate;
}

uint8_t read_humidity(void) {
    if (!heart_rate_sensor_initialized) return NO_HR;
    uint8_t humidity = sht3xdis_read_humidity();
    return humidity;
}

float read_temperature(temp_sensor_type_t sensor) {
    switch(sensor) {
        case BODY:
            if (!temperature_sensors_initialized.body) return NO_TEMP;
            return tmp117_read_temperature();
        case AMBIENT:
            if (!temperature_sensors_initialized.ambient) return NO_TEMP;
            return sht3xdis_read_temperature();
         default: 
            LOG_ERR("Invalid sensor type provided: %d", sensor);
            return;
    }
}


// Perform calibration routines
void calibrate_heart_rate_sensor(void) {
    heart_rate_sensor_calibrated = true;
}

void calibrate_temperature_sensor(temp_sensor_type_t sensor) {
    switch(sensor) {
        case BODY:
            temperature_sensors_calibrated.body = true;
            return;
        case AMBIENT:
            temperature_sensors_calibrated.ambient = true;
            return;
         default: 
            LOG_ERR("Invalid sensor type provided: %d", sensor);
            return;
    }
}

bool is_hr_sensor_ready(void) {
    return heart_rate_sensor_calibrated;
}

bool is_temp_sensor_ready(temp_sensor_type_t sensor) {
    switch(sensor) {
        case BODY:
            return temperature_sensors_calibrated.body;
        case AMBIENT:
            return temperature_sensors_calibrated.ambient;
         default: 
            LOG_ERR("Invalid sensor type provided: %d", sensor);
            return false;
    }
}