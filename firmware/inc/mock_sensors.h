/*
 * Mock Sensors Module
 * Header File
 */

#ifndef MOCK_SENSORS_H_
#define MOCK_SENSORS_H_

#include <zephyr/types.h>
#include <stdbool.h>
#include <config.h>
#include <sensors.h>
#include "state_manager.h"


/* ========================================================================== */
/* MOCK MODES                                                                 */
/* ========================================================================== */

typedef enum {
    MOCK_MODE_STATIC,     /* Fixed values */
    MOCK_MODE_RANDOM,     /* Random walk */
    MOCK_MODE_SINE_WAVE,  /* Sinusoidal variation */
    MOCK_MODE_SCENARIO    /* Scripted scenario playback */
} mock_mode_t;

/* ========================================================================== */
/* PREDEFINED SCENARIOS                                                       */
/* ========================================================================== */

typedef enum {
    SCENARIO_NONE = -1,
    SCENARIO_NORMAL = 0,        /* Normal day - all OK */
    SCENARIO_EXERCISE,          /* Exercise session - high HR, temp rise */
    SCENARIO_FEVER,             /* Fever developing - temp WARNING/EMERGENCY */
    SCENARIO_TACHYCARDIA,       /* Heart rate spike - HR WARNING/EMERGENCY */
    SCENARIO_BATTERY_DRAIN,     /* Battery draining to critical */
    SCENARIO_EMERGENCY,         /* Multi-parameter emergency */
    SCENARIO_COUNT
} mock_scenario_t;

/* ========================================================================== */
/* CONFIGURATION                                                              */
/* ========================================================================== */

typedef struct {
    mock_mode_t mode;
    uint8_t heart_rate;
    float temperature;
    uint16_t battery_voltage;
    bool noise_enabled;
    uint8_t noise_amplitude;
    button_status_t button_status;
} mock_sensor_config_t;

/* ========================================================================== */
/* INITIALIZATION                                                             */
/* ========================================================================== */

/**
 * @brief Initialize mock sensors
 * 
 * Sets default values: HR=72, Temp=36.5, Battery=3700mV
 */
void mock_sensors_init(void);

/* ========================================================================== */
/* CONFIGURATION                                                              */
/* ========================================================================== */

/**
 * @brief Set mock sensor mode
 * 
 * @param mode Operating mode
 */
void mock_sensors_set_mode(mock_mode_t mode);

/**
 * @brief Set static heart rate value
 * 
 * @param hr Heart rate in BPM (40-220)
 */
void mock_sensors_set_heart_rate(uint8_t hr);

/**
 * @brief Set static temperature value
 * 
 * @param temp Temperature in Celsius (35.0-40.0)
 */
void mock_sensors_set_temperature(float temp);

/**
 * @brief Set static battery voltage
 * 
 * @param voltage_mv Battery voltage in millivolts (3000-4200)
 */
void mock_sensors_set_battery(uint16_t voltage_mv);

/**
 * @brief Enable/disable noise on readings
 * 
 * Adds random variation to simulate real sensor noise.
 * 
 * @param enable true to enable noise
 * @param amplitude Noise amplitude (e.g., 5 = ±5 units)
 */
void mock_sensors_enable_noise(bool enable, uint8_t amplitude);

/* ========================================================================== */
/* SENSOR READING                                                             */
/* ========================================================================== */

/**
 * @brief Read mock heart rate
 * 
 * Returns value based on current mode and configuration.
 * 
 * @return Heart rate in BPM
 */
uint8_t mock_read_heart_rate(void);

/**
 * @brief Read mock temperature
 * 
 * @return Temperature in Celsius
 */
float mock_read_temperature(void);

/**
 * @brief Read mock battery voltage
 * 
 * @return Battery voltage in millivolts
 */
uint16_t mock_read_battery_voltage(void);

/* ========================================================================== */
/* SCENARIO PLAYBACK                                                          */
/* ========================================================================== */

/**
 * @brief Start a predefined scenario
 * 
 * Plays through a scripted sequence of sensor values.
 * Automatically transitions through states.
 * 
 * @param scenario Scenario to play
 */
void mock_sensors_start_scenario(mock_scenario_t scenario);

/**
 * @brief Stop current scenario
 * 
 * Returns to STATIC mode.
 */
void mock_sensors_stop_scenario(void);

/**
 * @brief Update scenario state
 * 
 * Call this periodically (e.g., every 100ms) to advance the scenario.
 * Automatically moves to next step when duration expires.
 */
void mock_sensors_update_scenario(void);

/**
 * @brief Get scenario name
 * 
 * @param scenario Scenario enum
 * @return Human-readable name
 */
const char* mock_sensors_get_scenario_name(mock_scenario_t scenario);

/* ========================================================================== */
/* DIAGNOSTICS                                                                */
/* ========================================================================== */

/**
 * @brief Print current mock sensor status
 * 
 * Logs current mode, values, and scenario state.
 */
void mock_sensors_print_status(void);

#endif /* MOCK_SENSORS_H_ */