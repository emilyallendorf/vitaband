/*
 * Mock Sensors Module
 * Simulates sensor readings for testing before hardware arrives
 * Supports scripted scenarios and interactive testing
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include "mock_sensors.h"
#include <math.h>

LOG_MODULE_REGISTER(mock_sensors, LOG_LEVEL_DBG);
#ifndef CLAMP
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))
#endif

/* ========================================================================== */
/* MOCK SENSOR STATE                                                          */
/* ========================================================================== */

static mock_sensor_config_t config = {
    .mode = MOCK_MODE_STATIC,
    .heart_rate = 72,
    .temperature = 36.5f,
    .battery_voltage = 3700,
    .noise_enabled = false,
    .noise_amplitude = 5,
    .button_status = UNPRESSED
};

static mock_scenario_t current_scenario = SCENARIO_NONE;
static uint32_t scenario_start_time = 0;

/* Scenario playback state */
static bool scenario_active = false;
static size_t scenario_step = 0;

/* ========================================================================== */
/* INITIALIZATION                                                             */
/* ========================================================================== */

void mock_sensors_init(void)
{
    LOG_DBG("Mock sensors initialized");
    LOG_DBG("Mode: STATIC, HR: %u BPM, Temp: %.1f°C, Battery: %u mV",
            config.heart_rate, config.temperature, config.battery_voltage);
}

/* ========================================================================== */
/* CONFIGURATION                                                              */
/* ========================================================================== */

void mock_sensors_set_mode(mock_mode_t mode)
{
    config.mode = mode;
    LOG_DBG("Mock mode set to: %d", mode);
}

void mock_sensors_set_heart_rate(uint8_t hr)
{
    config.heart_rate = hr;
    LOG_DBG("Mock HR set to: %u BPM", hr);
}

void mock_sensors_set_temperature(float temp)
{
    config.temperature = temp;
    LOG_DBG("Mock temp set to: %.1f°C", temp);
}

void mock_sensors_set_button_status(button_status_t status){
    config.button_status = status;
    LOG_DBG("Button status set");
}

void mock_sensors_set_battery(uint16_t voltage_mv)
{
    config.battery_voltage = voltage_mv;
    LOG_DBG("Mock battery set to: %u mV", voltage_mv);
}

void mock_sensors_enable_noise(bool enable, uint8_t amplitude)
{
    config.noise_enabled = enable;
    config.noise_amplitude = amplitude;
    LOG_DBG("Noise %s (amplitude: %u)", enable ? "enabled" : "disabled", amplitude);
}

/* ========================================================================== */
/* SENSOR READING WITH NOISE                                                  */
/* ========================================================================== */

static int32_t add_noise(int32_t value, uint8_t amplitude)
{
    if (!config.noise_enabled || amplitude == 0) {
        return value;
    }
    
    /* Generate random noise: -amplitude to +amplitude */
    int32_t noise = (sys_rand32_get() % (2 * amplitude + 1)) - amplitude;
    return value + noise;
}

uint8_t mock_read_heart_rate(void)
{
    int32_t hr = config.heart_rate;
    
    switch (config.mode) {
        case MOCK_MODE_STATIC:
            /* Just return configured value */
            break;
            
        case MOCK_MODE_RANDOM:
            /* Random walk: +/- 10 BPM */
            hr += (sys_rand32_get() % 21) - 10;
            hr = CLAMP(hr, 40, 220);
            config.heart_rate = hr;
            break;
            
        case MOCK_MODE_SINE_WAVE:
            /* Simulate breathing/activity variation */
            {
                uint32_t time_s = k_uptime_get_32() / 1000;
                float variation = 10.0f * sinf(2.0f * 3.14159f * time_s / 30.0f);
                hr = config.heart_rate + (int32_t)variation;
            }
            break;
            
        case MOCK_MODE_SCENARIO:
            /* Handled by scenario playback */
            break;
    }
    
    /* Add noise if enabled */
    hr = add_noise(hr, config.noise_amplitude);
    hr = CLAMP(hr, 0, 255);
    
    return (uint8_t)hr;
}

float mock_read_temperature(void)
{
    float temp = config.temperature;
    
    switch (config.mode) {
        case MOCK_MODE_STATIC:
            /* Just return configured value */
            break;
            
        case MOCK_MODE_RANDOM:
            /* Random walk: +/- 0.3°C */
            {
                int32_t delta = (sys_rand32_get() % 7) - 3;  /* -3 to +3 */
                temp += delta * 0.1f;
                temp = CLAMP(temp, 35.0f, 40.0f);
                config.temperature = temp;
            }
            break;
            
        case MOCK_MODE_SINE_WAVE:
            /* Simulate slow temperature drift */
            {
                uint32_t time_s = k_uptime_get_32() / 1000;
                float variation = 0.5f * sinf(2.0f * 3.14159f * time_s / 60.0f);
                temp = config.temperature + variation;
            }
            break;
            
        case MOCK_MODE_SCENARIO:
            /* Handled by scenario playback */
            break;
    }
    
    /* Add noise if enabled */
    if (config.noise_enabled) {
        int32_t noise = (sys_rand32_get() % (2 * config.noise_amplitude + 1)) - config.noise_amplitude;
        temp += noise * 0.01f;  /* Noise in 0.01°C increments */
    }
    
    return temp;
}

uint16_t mock_read_battery_voltage(void)
{
    uint16_t voltage = config.battery_voltage;
    
    switch (config.mode) {
        case MOCK_MODE_STATIC:
            /* Just return configured value */
            break;
            
        case MOCK_MODE_RANDOM:
            /* Random variation: +/- 50mV */
            voltage += (sys_rand32_get() % 101) - 50;
            voltage = CLAMP(voltage, 3000, 4200);
            config.battery_voltage = voltage;
            break;
            
        case MOCK_MODE_SINE_WAVE:
            /* Not really useful for battery */
            break;
            
        case MOCK_MODE_SCENARIO:
            /* Handled by scenario playback */
            break;
    }
    
    /* Add noise if enabled */
    voltage = add_noise(voltage, config.noise_amplitude);
    voltage = CLAMP(voltage, 2500, 4500);
    
    return voltage;
}

button_status_t mock_read_button_status(void) {
    return config.button_status;
}

/* ========================================================================== */
/* SCENARIO DEFINITIONS                                                       */
/* ========================================================================== */

/* Scenario step definition */
typedef struct {
    uint32_t duration_ms;  /* How long to hold these values */
    uint8_t heart_rate;
    float temperature;
    uint16_t battery_mv;
    const char *description;
} scenario_step_t;

/* Scenario: Normal day */
static const scenario_step_t scenario_normal[] = {
    { 5000,  72, 36.5f, 4200, "Morning - Fully charged, resting" },
    { 5000,  68, 36.4f, 4100, "Still resting, slight battery drain" },
    { 5000,  75, 36.6f, 4000, "Light activity" },
    { 5000,  70, 36.5f, 3900, "Back to rest" },
    { 5000,  72, 36.6f, 3800, "Evening" },
    { 0, 0, 0, 0, NULL }  /* End marker */
};

/* Scenario: Exercise session */
static const scenario_step_t scenario_exercise[] = {
    { 3000,  70, 36.5f, 3800, "Pre-exercise resting" },
    { 3000,  85, 36.8f, 3750, "Warm-up" },
    { 3000, 110, 37.2f, 3700, "Light exercise" },
    { 3000, 135, 37.8f, 3650, "Moderate exercise" },
    { 3000, 155, 38.2f, 3600, "High intensity" },
    { 3000, 145, 38.0f, 3550, "Sustaining" },
    { 3000, 120, 37.5f, 3500, "Cool down" },
    { 3000,  95, 37.2f, 3450, "Recovery" },
    { 3000,  75, 36.8f, 3400, "Post-exercise" },
    { 0, 0, 0, 0, NULL }
};

/* Scenario: Fever developing */
static const scenario_step_t scenario_fever[] = {
    { 4000,  72, 36.5f, 3700, "Normal baseline" },
    { 4000,  75, 36.8f, 3680, "Slight temperature rise" },
    { 4000,  78, 37.3f, 3660, "Low-grade fever starting" },
    { 4000,  82, 37.9f, 3640, "Fever rising - WARNING" },
    { 4000,  88, 38.5f, 3620, "High fever - WARNING" },
    { 4000,  92, 39.2f, 3600, "Very high fever - EMERGENCY" },
    { 4000,  95, 39.5f, 3580, "Critical fever - EMERGENCY" },
    { 0, 0, 0, 0, NULL }
};

/* Scenario: Tachycardia episode */
static const scenario_step_t scenario_tachycardia[] = {
    { 3000,  72, 36.5f, 3700, "Normal resting" },
    { 3000,  85, 36.6f, 3680, "Heart rate increasing" },
    { 3000, 105, 36.7f, 3660, "Elevated HR" },
    { 3000, 125, 36.8f, 3640, "Tachycardia - WARNING" },
    { 3000, 145, 36.9f, 3620, "High tachycardia - WARNING" },
    { 3000, 165, 37.0f, 3600, "Severe tachycardia - EMERGENCY" },
    { 3000, 155, 36.9f, 3580, "Decreasing" },
    { 3000, 135, 36.8f, 3560, "Still elevated" },
    { 3000, 110, 36.7f, 3540, "Returning to normal" },
    { 3000,  85, 36.6f, 3520, "Recovery" },
    { 0, 0, 0, 0, NULL }
};

/* Scenario: Battery drain */
static const scenario_step_t scenario_battery_drain[] = {
    { 3000,  72, 36.5f, 4200, "100% - Full charge" },
    { 3000,  72, 36.5f, 4000, "85% - Good" },
    { 3000,  72, 36.5f, 3800, "60% - Good" },
    { 3000,  72, 36.5f, 3600, "30% - Getting low" },
    { 3000,  72, 36.5f, 3400, "10% - Low battery warning" },
    { 3000,  72, 36.5f, 3200, "5% - Critical warning" },
    { 3000,  72, 36.5f, 3050, "0% - Should shut down" },
    { 0, 0, 0, 0, NULL }
};

/* Scenario: Multi-parameter emergency */
static const scenario_step_t scenario_emergency[] = {
    { 4000,  72, 36.5f, 3700, "Starting normal" },
    { 4000,  85, 37.0f, 3680, "Slight elevation" },
    { 4000, 105, 37.8f, 3660, "Both increasing" },
    { 4000, 128, 38.5f, 3640, "WARNING - both high" },
    { 4000, 145, 39.0f, 3620, "EMERGENCY - fever + tachycardia" },
    { 4000, 160, 39.5f, 3600, "CRITICAL - both very high" },
    { 0, 0, 0, 0, NULL }
};

/* Scenario lookup table */
static const scenario_step_t *scenario_table[] = {
    [SCENARIO_NORMAL] = scenario_normal,
    [SCENARIO_EXERCISE] = scenario_exercise,
    [SCENARIO_FEVER] = scenario_fever,
    [SCENARIO_TACHYCARDIA] = scenario_tachycardia,
    [SCENARIO_BATTERY_DRAIN] = scenario_battery_drain,
    [SCENARIO_EMERGENCY] = scenario_emergency
};

/* ========================================================================== */
/* SCENARIO PLAYBACK                                                          */
/* ========================================================================== */

void mock_sensors_start_scenario(mock_scenario_t scenario)
{
    if (scenario >= SCENARIO_COUNT) {
        LOG_ERR("Invalid scenario: %d", scenario);
        return;
    }
    
    current_scenario = scenario;
    scenario_active = true;
    scenario_step = 0;
    scenario_start_time = k_uptime_get_32();
    config.mode = MOCK_MODE_SCENARIO;
    
    /* Load first step */
    const scenario_step_t *step = &scenario_table[scenario][0];
    config.heart_rate = step->heart_rate;
    config.temperature = step->temperature;
    config.battery_voltage = step->battery_mv;
    
    LOG_DBG("=== Starting Scenario: %d ===", scenario);
    LOG_DBG("Step 0: %s", step->description);
}

void mock_sensors_stop_scenario(void)
{
    scenario_active = false;
    current_scenario = SCENARIO_NONE;
    config.mode = MOCK_MODE_STATIC;
    LOG_DBG("Scenario stopped");
}

bool mock_sensors_scenario_active(void)
{
	return scenario_active;
}

void mock_sensors_update_scenario(void)
{
    if (!scenario_active || current_scenario >= SCENARIO_COUNT) {
        return;
    }
    
    const scenario_step_t *steps = scenario_table[current_scenario];
    const scenario_step_t *current_step = &steps[scenario_step];
    
    /* Check if current step is complete */
    uint32_t elapsed = k_uptime_get_32() - scenario_start_time;
    
    if (elapsed >= current_step->duration_ms) {
        /* Move to next step */
        scenario_step++;
        scenario_start_time = k_uptime_get_32();
        
        const scenario_step_t *next_step = &steps[scenario_step];
        
        /* Check for end of scenario */
        if (next_step->duration_ms == 0) {
            LOG_DBG("=== Scenario Complete ===");
            mock_sensors_stop_scenario();
            return;
        }
        
        /* Load next step */
        config.heart_rate = next_step->heart_rate;
        config.temperature = next_step->temperature;
        config.battery_voltage = next_step->battery_mv;
        
        LOG_DBG("Step %zu: %s (HR=%u, Temp=%.1f, Batt=%u)",
                scenario_step,
                next_step->description,
                next_step->heart_rate,
                next_step->temperature,
                next_step->battery_mv);
    }
}

/* ========================================================================== */
/* STATUS REPORTING                                                           */
/* ========================================================================== */

void mock_sensors_print_status(void)
{
    LOG_DBG("=== Mock Sensor Status ===");
    LOG_DBG("Mode: %d", config.mode);
    LOG_DBG("HR: %u BPM", config.heart_rate);
    LOG_DBG("Temp: %.1f°C", config.temperature);
    LOG_DBG("Battery: %u mV", config.battery_voltage);
    LOG_DBG("Noise: %s (amplitude: %u)",
            config.noise_enabled ? "ON" : "OFF",
            config.noise_amplitude);
    
    if (scenario_active) {
        LOG_DBG("Active scenario: %d, step: %zu", current_scenario, scenario_step);
    }
}

const char* mock_sensors_get_scenario_name(mock_scenario_t scenario)
{
    switch (scenario) {
        case SCENARIO_NORMAL:        return "Normal Day";
        case SCENARIO_EXERCISE:      return "Exercise Session";
        case SCENARIO_FEVER:         return "Fever Development";
        case SCENARIO_TACHYCARDIA:   return "Tachycardia Episode";
        case SCENARIO_BATTERY_DRAIN: return "Battery Drain";
        case SCENARIO_EMERGENCY:     return "Multi-Parameter Emergency";
        default:                     return "Unknown";
    }
}