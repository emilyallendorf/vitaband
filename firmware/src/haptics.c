/*
 * Haptics Module - Alert System
 * Controls LED, Vibration Motor, and Buzzer
 * Supports user-configurable alerts per device state
 */

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "haptics.h"

#if DT_HAS_ALIAS(led0) && DT_NODE_HAS_STATUS(DT_ALIAS(led0), okay)
#define HAPTICS_HAS_LED 1
#else
#define HAPTICS_HAS_LED 0
#endif

LOG_MODULE_REGISTER(haptics, LOG_LEVEL_INF);

/* ========================================================================== */
/* HARDWARE SPECIFICATIONS                                                    */
/* ========================================================================== */

// FIXME: Double check these specs
/*
 * Vibration Motor: 310-101 10mm Shaftless
 * - Nominal Voltage: 3V
 * - Voltage Range: 2.5V - 3.8V
 * - Current: 75mA typical, 85mA start
 * - Start Voltage: 2.3V
 * - Frequency: 11,000 RPM (183 Hz)
 * 
 * Buzzer: CPT-9019B-SMT-TR Piezo
 * - Rated Voltage: 3V peak-to-peak
 * - Operating Voltage: 1V - 25V peak-to-peak
 * - Resonant Frequency: 4000 Hz
 * - Current: ~1mA at rated voltage
 * - Sound Level: 75 dB @ 10cm
 */

/* ========================================================================== */
/* GPIO CONFIGURATION                                                         */
/* ========================================================================== */

#if HAPTICS_HAS_LED
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif

/* MOTOR_EN — GPIO; intensity maps to on/off only */
static const struct gpio_dt_spec vibration = GPIO_DT_SPEC_GET(DT_ALIAS(vibration_motor), gpios);

/* BUZZER_EN — GPIO; frequency arg ignored */
static const struct gpio_dt_spec buzzer = GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);

/* ========================================================================== */
/* ALERT PATTERNS                                                             */
/* ========================================================================== */

/* Default alert patterns for each state */
static haptic_pattern_t default_patterns[NUM_DEVICE_STATES] = {
    /* OK */
    {
        .led_enabled = true,
        .led_pattern = LED_STEADY,
        .led_duration_ms = 0,  /* Always on */
        
        .vibration_enabled = false,
        .vibration_intensity = 0,
        .vibration_duration_ms = 0,
        
        .buzzer_enabled = false,
        .buzzer_frequency_hz = 0,
        .buzzer_duration_ms = 0
    },
    /* WARNING */
    {
        .led_enabled = true,
        .led_pattern = LED_SLOW_BLINK,
        .led_duration_ms = 0,  /* Continuous */
        
        .vibration_enabled = false,
        .vibration_intensity = 0,  /* 50% intensity */
        .vibration_duration_ms = 0,  /* Single pulse */
        
        .buzzer_enabled = false,
        .buzzer_frequency_hz = 0,  
        .buzzer_duration_ms = 0
    },

    
    /* CRITICAL */
    {
        .led_enabled = true,
        .led_pattern = LED_SLOW_BLINK,
        .led_duration_ms = 0,  /* Continuous */
        
        .vibration_enabled = true,
        .vibration_intensity = 50,  /* 50% intensity */
        .vibration_duration_ms = 200,  /* Single pulse */
        
        .buzzer_enabled = false,
        .buzzer_frequency_hz = 0,  
        .buzzer_duration_ms = 0
    },
    
    /* STATE_EMERGENCY */
    {
        .led_enabled = true,
        .led_pattern = LED_FAST_BLINK,
        .led_duration_ms = 0,  /* Continuous */
        
        .vibration_enabled = true,
        .vibration_intensity = 100,  /* Full intensity */
        .vibration_duration_ms = 500,  /* Longer pulse */
        
        .buzzer_enabled = true,
        .buzzer_frequency_hz = 4000,  /* Resonant freq for max volume */
        .buzzer_duration_ms = 500
    }
};

/* Current user settings (starts as defaults) */
static haptic_pattern_t user_patterns[NUM_DEVICE_STATES];

/* ========================================================================== */
/* LED CONTROL                                                                */
/* ========================================================================== */

#if HAPTICS_HAS_LED
static struct k_work_delayable led_blink_work;
static bool led_blink_active = false;
static led_pattern_t current_led_pattern = LED_OFF;
static bool led_state = false;
#endif

static void led_blink_handler(struct k_work *work)
{
#if !HAPTICS_HAS_LED
	ARG_UNUSED(work);
	return;
#else
	if (!led_blink_active) {
		return;
	}

	/* Toggle LED */
	led_state = !led_state;
	gpio_pin_set_dt(&led, led_state);
    
    /* Determine next delay based on pattern */
    uint32_t delay_ms;
    
    switch (current_led_pattern) {
        case LED_SLOW_BLINK:
            delay_ms = 1000;  /* 1 Hz */
            break;
            
        case LED_FAST_BLINK:
            delay_ms = 250;   /* 4 Hz */
            break;
            
        case LED_PULSE:
            /* Short on, long off */
            delay_ms = led_state ? 100 : 900;
            break;
            
        default:
            return;  /* Stop blinking */
    }
    
	k_work_schedule(&led_blink_work, K_MSEC(delay_ms));
#endif
}

static void led_set(bool on)
{
#if HAPTICS_HAS_LED
	gpio_pin_set_dt(&led, on);
#else
	ARG_UNUSED(on);
#endif
}

static void led_start_pattern(led_pattern_t pattern)
{
#if HAPTICS_HAS_LED
	led_blink_active = false;
	k_work_cancel_delayable(&led_blink_work);

	current_led_pattern = pattern;

	switch (pattern) {
	case LED_OFF:
		led_set(false);
		break;

	case LED_STEADY:
		led_set(true);
		break;

	case LED_SLOW_BLINK:
	case LED_FAST_BLINK:
	case LED_PULSE:
		led_blink_active = true;
		led_state = true;
		led_set(true);
		k_work_schedule(&led_blink_work,
				K_MSEC(pattern == LED_FAST_BLINK ? 250 : 1000));
		break;
	}
#else
	ARG_UNUSED(pattern);
#endif
}

/* ========================================================================== */
/* VIBRATION MOTOR CONTROL                                                    */
/* ========================================================================== */

/* Vibration duration timer */
static struct k_work_delayable vibration_stop_work;

static void vibration_stop_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	(void)gpio_pin_set_dt(&vibration, 0);
	LOG_DBG("Vibration stopped");
}

static void vibration_set_intensity(uint8_t intensity_percent)
{
	if (intensity_percent > 100) {
		intensity_percent = 100;
	}
	(void)gpio_pin_set_dt(&vibration, intensity_percent > 0 ? 1 : 0);
}

static void vibration_start(uint8_t intensity, uint16_t duration_ms)
{
    /* Cancel any existing vibration */
    k_work_cancel_delayable(&vibration_stop_work);
    
    /* Start vibration */
    vibration_set_intensity(intensity);
    LOG_DBG("Vibration started: %u%% for %u ms", intensity, duration_ms);
    
    /* Schedule stop if duration specified */
    if (duration_ms > 0) {
        k_work_schedule(&vibration_stop_work, K_MSEC(duration_ms));
    }
}

static void vibration_stop(void)
{
    k_work_cancel_delayable(&vibration_stop_work);
    vibration_set_intensity(0);
}

/* ========================================================================== */
/* BUZZER CONTROL                                                             */
/* ========================================================================== */

/* Buzzer duration timer */
static struct k_work_delayable buzzer_stop_work;

static void buzzer_stop_handler(struct k_work *work)
{
	(void)gpio_pin_set_dt(&buzzer, 0);
	LOG_DBG("Buzzer stopped");
}

static void buzzer_set_tone(uint16_t frequency_hz)
{
	if (frequency_hz == 0) {
		(void)gpio_pin_set_dt(&buzzer, 0);
		return;
	}
	(void)gpio_pin_set_dt(&buzzer, 1);
}

static void buzzer_start(uint16_t frequency_hz, uint16_t duration_ms)
{
    /* Cancel any existing tone */
    k_work_cancel_delayable(&buzzer_stop_work);
    
    /* Start tone */
    buzzer_set_tone(frequency_hz);
    LOG_DBG("Buzzer started: %u Hz for %u ms", frequency_hz, duration_ms);
    
    /* Schedule stop if duration specified */
    if (duration_ms > 0) {
        k_work_schedule(&buzzer_stop_work, K_MSEC(duration_ms));
    }
}

static void buzzer_stop(void)
{
    k_work_cancel_delayable(&buzzer_stop_work);
    buzzer_set_tone(0);
}

/* ========================================================================== */
/* INITIALIZATION                                                             */
/* ========================================================================== */

int haptics_init(void)
{
    int ret;
    
	LOG_INF("Initializing haptics system");

#if HAPTICS_HAS_LED
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED GPIO not ready");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Failed to configure LED: %d", ret);
		return ret;
	}
#endif

	if (!gpio_is_ready_dt(&vibration)) {
		LOG_ERR("Motor enable GPIO not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&vibration, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		LOG_ERR("Motor GPIO configure failed: %d", ret);
		return ret;
	}
    
    /* Buzzer enable GPIO */
    if (!gpio_is_ready_dt(&buzzer)) {
        LOG_ERR("Buzzer GPIO not ready");
        return -ENODEV;
    }
    ret = gpio_pin_configure_dt(&buzzer, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        LOG_ERR("Buzzer GPIO configure failed: %d", ret);
        return ret;
    }
    
	/* Initialize work items */
#if HAPTICS_HAS_LED
	k_work_init_delayable(&led_blink_work, led_blink_handler);
#endif
	k_work_init_delayable(&vibration_stop_work, vibration_stop_handler);
	k_work_init_delayable(&buzzer_stop_work, buzzer_stop_handler);
    
    /* Copy default patterns to user settings */
    memcpy(user_patterns, default_patterns, sizeof(default_patterns));
    
    /* Turn everything off initially */
    haptics_stop_all();
    
    LOG_INF("Haptics initialized");
    return 0;
}

/* ========================================================================== */
/* PATTERN MANAGEMENT                                                         */
/* ========================================================================== */

void haptics_set_pattern(device_state_t state, const haptic_pattern_t *pattern)
{
    if (state >= NUM_DEVICE_STATES || pattern == NULL) {
        LOG_ERR("Invalid state or pattern");
        return;
    }
    
    memcpy(&user_patterns[state], pattern, sizeof(haptic_pattern_t));
    LOG_INF("Updated pattern for state %d", state);
}

void haptics_get_pattern(device_state_t state, haptic_pattern_t *pattern)
{
    if (state >= NUM_DEVICE_STATES || pattern == NULL) {
        LOG_ERR("Invalid state or pattern");
        return;
    }
    
    memcpy(pattern, &user_patterns[state], sizeof(haptic_pattern_t));
}

void haptics_reset_to_defaults(void)
{
    memcpy(user_patterns, default_patterns, sizeof(default_patterns));
    LOG_INF("Reset all patterns to defaults");
}

/* ========================================================================== */
/* ALERT TRIGGERING                                                           */
/* ========================================================================== */

void haptics_trigger_alert(device_state_t state)
{
    if (state >= NUM_DEVICE_STATES) {
        LOG_ERR("Invalid state: %d", state);
        return;
    }
    
    haptic_pattern_t *pattern = &user_patterns[state];
    
    LOG_INF("Triggering alert for state %d", state);
    
    /* LED */
    if (pattern->led_enabled) {
        led_start_pattern(pattern->led_pattern);
    } else {
        led_start_pattern(LED_OFF);
    }
    
    /* Vibration */
    if (pattern->vibration_enabled) {
        vibration_start(pattern->vibration_intensity, 
                       pattern->vibration_duration_ms);
    } else {
        vibration_stop();
    }
    
    /* Buzzer */
    if (pattern->buzzer_enabled) {
        buzzer_start(pattern->buzzer_frequency_hz,
                    pattern->buzzer_duration_ms);
    } else {
        buzzer_stop();
    }
}

/* ========================================================================== */
/* MANUAL CONTROL (for testing or special cases)                             */
/* ========================================================================== */

void haptics_led_set(bool on)
{
    led_start_pattern(on ? LED_STEADY : LED_OFF);
}

void haptics_led_blink(led_pattern_t pattern)
{
    led_start_pattern(pattern);
}

void haptics_vibration_pulse(uint8_t intensity, uint16_t duration_ms)
{
    vibration_start(intensity, duration_ms);
}

void haptics_buzzer_beep(uint16_t frequency_hz, uint16_t duration_ms)
{
    buzzer_start(frequency_hz, duration_ms);
}

void haptics_stop_all(void)
{
    led_start_pattern(LED_OFF);
    vibration_stop();
    buzzer_stop();
    LOG_DBG("All haptics stopped");
}

/* ========================================================================== */
/* PATTERN SERIALIZATION (for BLE settings)                                  */
/* ========================================================================== */

void haptics_pattern_to_bytes(const haptic_pattern_t *pattern, uint8_t *buffer)
{
    if (pattern == NULL || buffer == NULL) {
        return;
    }
    
    /* Pack pattern into byte array for BLE transmission
     * Format (11 bytes total):
     * [0]    LED enabled (bool)
     * [1]    LED pattern (enum)
     * [2-3]  LED duration (uint16_t)
     * [4]    Vibration enabled (bool)
     * [5]    Vibration intensity (uint8_t)
     * [6-7]  Vibration duration (uint16_t)
     * [8]    Buzzer enabled (bool)
     * [9-10] Buzzer frequency (uint16_t)
     * [11-12] Buzzer duration (uint16_t)
     */
    
    buffer[0] = pattern->led_enabled ? 1 : 0;
    buffer[1] = pattern->led_pattern;
    buffer[2] = (pattern->led_duration_ms >> 8) & 0xFF;
    buffer[3] = pattern->led_duration_ms & 0xFF;
    
    buffer[4] = pattern->vibration_enabled ? 1 : 0;
    buffer[5] = pattern->vibration_intensity;
    buffer[6] = (pattern->vibration_duration_ms >> 8) & 0xFF;
    buffer[7] = pattern->vibration_duration_ms & 0xFF;
    
    buffer[8] = pattern->buzzer_enabled ? 1 : 0;
    buffer[9] = (pattern->buzzer_frequency_hz >> 8) & 0xFF;
    buffer[10] = pattern->buzzer_frequency_hz & 0xFF;
    buffer[11] = (pattern->buzzer_duration_ms >> 8) & 0xFF;
    buffer[12] = pattern->buzzer_duration_ms & 0xFF;
}

void haptics_pattern_from_bytes(haptic_pattern_t *pattern, const uint8_t *buffer)
{
    if (pattern == NULL || buffer == NULL) {
        return;
    }
    
    pattern->led_enabled = buffer[0] != 0;
    pattern->led_pattern = (led_pattern_t)buffer[1];
    pattern->led_duration_ms = ((uint16_t)buffer[2] << 8) | buffer[3];
    
    pattern->vibration_enabled = buffer[4] != 0;
    pattern->vibration_intensity = buffer[5];
    pattern->vibration_duration_ms = ((uint16_t)buffer[6] << 8) | buffer[7];
    
    pattern->buzzer_enabled = buffer[8] != 0;
    pattern->buzzer_frequency_hz = ((uint16_t)buffer[9] << 8) | buffer[10];
    pattern->buzzer_duration_ms = ((uint16_t)buffer[11] << 8) | buffer[12];
}

/* ========================================================================== */
/* DIAGNOSTICS                                                                */
/* ========================================================================== */

void haptics_test_all(void)
{
    LOG_INF("=== Haptics Test Sequence ===");
    
    /* Test LED */
    LOG_INF("Testing LED...");
    haptics_led_set(true);
    k_msleep(500);
    haptics_led_set(false);
    k_msleep(500);
    
    /* Test vibration */
    LOG_INF("Testing vibration motor...");
    haptics_vibration_pulse(100, 500);
    k_msleep(1000);
    
    /* Test buzzer at different frequencies */
    LOG_INF("Testing buzzer - 1 kHz");
    haptics_buzzer_beep(1000, 300);
    k_msleep(500);
    
    LOG_INF("Testing buzzer - 2 kHz");
    haptics_buzzer_beep(2000, 300);
    k_msleep(500);
    
    LOG_INF("Testing buzzer - 4 kHz (resonant)");
    haptics_buzzer_beep(4000, 300);
    k_msleep(500);
    
    LOG_INF("Haptics test complete");
}

void haptics_print_pattern(device_state_t state)
{
    if (state >= NUM_DEVICE_STATES) {
        return;
    }
    
    haptic_pattern_t *p = &user_patterns[state];
    
    LOG_INF("=== Pattern for state %d ===", state);
    LOG_INF("LED: %s, Pattern: %d, Duration: %u ms",
            p->led_enabled ? "ON" : "OFF",
            p->led_pattern,
            p->led_duration_ms);
    LOG_INF("Vibration: %s, Intensity: %u%%, Duration: %u ms",
            p->vibration_enabled ? "ON" : "OFF",
            p->vibration_intensity,
            p->vibration_duration_ms);
    LOG_INF("Buzzer: %s, Frequency: %u Hz, Duration: %u ms",
            p->buzzer_enabled ? "ON" : "OFF",
            p->buzzer_frequency_hz,
            p->buzzer_duration_ms);
}