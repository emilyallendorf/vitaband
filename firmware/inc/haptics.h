/*
 * Haptics Module - Alert System
 * Header File
 */

#ifndef HAPTICS_H_
#define HAPTICS_H_

#include <zephyr/types.h>
#include <stdbool.h>

/* Include state manager for device_state_t */
/* If you don't have state_manager.h yet, define states here: */
typedef enum {
    STATE_OK = 0,
    STATE_WARNING = 1,
    STATE_EMERGENCY = 2,
    NUM_DEVICE_STATES = 3
} device_state_t;

/* ========================================================================== */
/* LED PATTERNS                                                               */
/* ========================================================================== */

typedef enum {
    LED_OFF = 0,          /* LED off */
    LED_STEADY,           /* Solid on */
    LED_SLOW_BLINK,       /* Blink at 1 Hz (500ms on, 500ms off) */
    LED_FAST_BLINK,       /* Blink at 4 Hz (125ms on, 125ms off) */
    LED_PULSE             /* Short pulse (100ms on, 900ms off) */
} led_pattern_t;

/* ========================================================================== */
/* HAPTIC PATTERN STRUCTURE                                                   */
/* ========================================================================== */

typedef struct {
    /* LED Configuration */
    bool led_enabled;
    led_pattern_t led_pattern;
    uint16_t led_duration_ms;  /* 0 = continuous */
    
    /* Vibration Configuration */
    bool vibration_enabled;
    uint8_t vibration_intensity;  /* 0-100% */
    uint16_t vibration_duration_ms;
    
    /* Buzzer Configuration */
    bool buzzer_enabled;
    uint16_t buzzer_frequency_hz;  /* 1000-8000 Hz typical, 4000 Hz = loudest */
    uint16_t buzzer_duration_ms;
} haptic_pattern_t;

/* ========================================================================== */
/* INITIALIZATION                                                             */
/* ========================================================================== */

/**
 * @brief Initialize haptics system
 * 
 * Sets up GPIO for LED and PWM for vibration motor and buzzer.
 * Loads default alert patterns.
 * 
 * @return 0 on success, negative errno on failure
 */
int haptics_init(void);

/* ========================================================================== */
/* PATTERN MANAGEMENT                                                         */
/* ========================================================================== */

/**
 * @brief Set alert pattern for a device state
 * 
 * Allows user to customize which alerts (LED, vibration, buzzer)
 * are triggered for each health state.
 * 
 * @param state Device state (OK, WARNING, EMERGENCY)
 * @param pattern Alert pattern configuration
 */
void haptics_set_pattern(device_state_t state, const haptic_pattern_t *pattern);

/**
 * @brief Get current alert pattern for a state
 * 
 * @param state Device state
 * @param pattern Output buffer for pattern
 */
void haptics_get_pattern(device_state_t state, haptic_pattern_t *pattern);

/**
 * @brief Reset all patterns to factory defaults
 * 
 * OK: LED steady, no vibration, no buzzer
 * WARNING: LED slow blink, brief vibration, 2kHz beep
 * EMERGENCY: LED fast blink, strong vibration, 4kHz beep
 */
void haptics_reset_to_defaults(void);

/* ========================================================================== */
/* ALERT TRIGGERING                                                           */
/* ========================================================================== */

/**
 * @brief Trigger alert for a device state
 * 
 * Activates LED, vibration, and/or buzzer according to
 * the configured pattern for this state.
 * 
 * Call this when transitioning to a new state.
 * 
 * @param state Device state to alert for
 */
void haptics_trigger_alert(device_state_t state);

/**
 * @brief Stop all active alerts
 * 
 * Turns off LED, vibration, and buzzer.
 * Useful when manually clearing an alert.
 */
void haptics_stop_all(void);

/* ========================================================================== */
/* MANUAL CONTROL (for testing or special alerts)                            */
/* ========================================================================== */

/**
 * @brief Manually control LED
 * 
 * @param on true = LED on, false = LED off
 */
void haptics_led_set(bool on);

/**
 * @brief Start LED blink pattern
 * 
 * @param pattern Blink pattern (SLOW, FAST, PULSE)
 */
void haptics_led_blink(led_pattern_t pattern);

/**
 * @brief Trigger vibration pulse
 * 
 * @param intensity Vibration strength (0-100%)
 * @param duration_ms How long to vibrate (0 = until stopped)
 */
void haptics_vibration_pulse(uint8_t intensity, uint16_t duration_ms);

/**
 * @brief Trigger buzzer beep
 * 
 * @param frequency_hz Tone frequency (4000 Hz = resonant/loudest)
 * @param duration_ms How long to beep (0 = until stopped)
 */
void haptics_buzzer_beep(uint16_t frequency_hz, uint16_t duration_ms);

/* ========================================================================== */
/* PATTERN SERIALIZATION (for BLE settings)                                  */
/* ========================================================================== */

/**
 * @brief Convert pattern to byte array
 * 
 * Packs haptic pattern into 13 bytes for BLE transmission.
 * Use this when sending settings to phone app.
 * 
 * @param pattern Pattern to serialize
 * @param buffer Output buffer (must be >= 13 bytes)
 */
void haptics_pattern_to_bytes(const haptic_pattern_t *pattern, uint8_t *buffer);

/**
 * @brief Convert byte array to pattern
 * 
 * Unpacks haptic pattern from 13-byte BLE data.
 * Use this when receiving settings from phone app.
 * 
 * @param pattern Output pattern
 * @param buffer Input buffer (must be 13 bytes)
 */
void haptics_pattern_from_bytes(haptic_pattern_t *pattern, const uint8_t *buffer);

/* ========================================================================== */
/* DIAGNOSTICS                                                                */
/* ========================================================================== */

/**
 * @brief Test all haptic outputs
 * 
 * Runs a test sequence:
 * - Blinks LED
 * - Pulses vibration
 * - Beeps buzzer at different frequencies
 * 
 * Useful for hardware verification.
 */
void haptics_test_all(void);

/**
 * @brief Print pattern configuration
 * 
 * Logs the current alert pattern for a state.
 * 
 * @param state Device state to print
 */
void haptics_print_pattern(device_state_t state);

#endif /* HAPTICS_H_ */