/*
 * Power Management Module
 * Integrated battery monitoring and power mode control
 * For Zephyr RTOS on nRF52840
 */
 
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/policy.h>
#include <zephyr/logging/log.h>
#include "power.h"
 
LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

/* ========================================================================== */
/* ADC CONFIGURATION FOR BATTERY MONITORING                                  */
/* ========================================================================== */
// TODO: assign the macros accordingly
#define ADC_RESOLUTION      12
#define ADC_GAIN            ADC_GAIN_1_6
#define ADC_REFERENCE       ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#define ADC_CHANNEL_ID      1

/* Voltage divider ratio */
//FIXME: adjust this based on the pcb, if we don't need this we can get rid of it
#define BATTERY_VOLTAGE_DIVIDER_RATIO  2.0f

/* Battery thresholds in mV */
//FIXME: not sure based on the datasheet if these can map to certain percentages
#define BATTERY_FULL        4200 //completely charged
#define BATTERY_GOOD        3700
#define BATTERY_LOW         3300
#define BATTERY_CRITICAL    3000 //completely discharged

/* ADC device and buffers (structs defined by the sdk in devices.h and adc.h)*/
static const struct device *adc_dev;
static struct adc_channel_cfg channel_cfg = {
    .gain = ADC_GAIN,
    .reference = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id = ADC_CHANNEL_ID,
#ifdef CONFIG_ADC_NRFX_SAADC
    .input_positive = NRF_SAADC_AIN1,
#endif
};
static int16_t sample_buffer;
static struct adc_sequence sequence = {
    .channels = BIT(ADC_CHANNEL_ID),
    .buffer = &sample_buffer,
    .buffer_size = sizeof(sample_buffer),
    .resolution = ADC_RESOLUTION,
};


static power_mode_t current_power_mode = POWER_MODE_ACTIVE;


void init_system_clock(void) {
    /* Zephyr handles system clock automatically */
    /* No additional initialization or systick logic needed */
    LOG_INF("System clock initialized (using Zephyr uptime)");
}

uint32_t get_system_time_ms(void) {
    return k_uptime_get_32();
}

static int battery_adc_init(void)
{
    int ret;
    /* Get ADC device */
    // TODO: not sure how this compiles with the app overlay, there may be a bug here
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC device not ready");
        return -ENODEV;
    }
    /* Configure ADC channel */
    ret = adc_channel_setup(adc_dev, &channel_cfg);
    if (ret != 0) {
        LOG_ERR("ADC channel setup failed: %d", ret);
        return ret;
    }
    return 0;
}

uint16_t get_battery_voltage_mv(void)
{
    int ret;
    if (adc_dev == NULL) {
        LOG_ERR("ADC not initialized");
        return 0;
    }
 
    /* Read ADC */
    ret = adc_read(adc_dev, &sequence);
    if (ret != 0) {
        LOG_ERR("ADC read failed: %d", ret);
        return 0;
    }
 
    int32_t adc_value = sample_buffer;

    /* Convert to voltage at ADC pin
     * V_adc = (adc_value / 4096) * 3600mV */
    int32_t voltage_at_adc = (adc_value * 3600) / 4096;
    /* Account for voltage divider */
    int32_t battery_voltage = (int32_t)(voltage_at_adc * BATTERY_VOLTAGE_DIVIDER_RATIO);
 
    LOG_DBG("ADC: %d, V_adc: %d mV, V_bat: %d mV", 
            adc_value, voltage_at_adc, battery_voltage);
 
    return (uint16_t)battery_voltage;
}

uint8_t get_battery_percentage(void) {
    // Read battery voltage via ADC
    // Convert to percentage based on battery chemistry
    uint16_t voltage_mv = get_battery_voltage_mv();
 
    // FIXME: not sure wether or not non-linear or linear is better based off the datasheet
    /* LiPo discharge curve (non-linear) */
    if (voltage_mv >= 4200) return 100;
    if (voltage_mv >= 4100) return 95;
    if (voltage_mv >= 4000) return 85;
    if (voltage_mv >= 3900) return 75;
    if (voltage_mv >= 3800) return 60;
    if (voltage_mv >= 3700) return 45;
    if (voltage_mv >= 3600) return 30;
    if (voltage_mv >= 3500) return 20;
    if (voltage_mv >= 3400) return 10;
    if (voltage_mv >= 3300) return 5;
    if (voltage_mv >= 3200) return 2;
    if (voltage_mv <= 3000) return 0;
 
    /* Linear interpolation for values in between */
    uint8_t percentage = ((voltage_mv - 3000) * 100) / 1200;
    return percentage;
}

bool is_battery_low(void) {
    uint16_t voltage = get_battery_voltage_mv();
    return (voltage < BATTERY_LOW && voltage >= BATTERY_CRITICAL);
}

bool is_battery_critical(void) {
    uint16_t voltage = get_battery_voltage_mv();
    return (voltage < BATTERY_CRITICAL);
}
 
bool is_battery_full(void) {
    uint16_t voltage = get_battery_voltage_mv();
    return (voltage >= BATTERY_FULL);
}


void set_power_mode(power_mode_t mode)
{
    switch (mode) {
        case POWER_MODE_ACTIVE:
            /* Full power mode - all peripherals active */
            /* Prevent any sleep states */
            pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
            pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
            pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
            LOG_DBG("Power mode: ACTIVE");
            break;
            
        case POWER_MODE_IDLE:
            /* CPU sleep, peripherals active */
            /* Allow light sleep (System ON Idle) */
            pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
            pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
            pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
            LOG_DBG("Power mode: IDLE");
            break;
            
        case POWER_MODE_SLEEP:
            /* Deep sleep - most peripherals off */
            /* Allow deeper sleep states */
            pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
            pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
            pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
            LOG_DBG("Power mode: SLEEP");
            break;
            
        case POWER_MODE_SHUTDOWN:
            /* Lowest power - System OFF */
            /* This will actually power down the system */
            LOG_INF("Entering SHUTDOWN mode");
            pm_state_force(0u, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
            break;
    }
    
    current_power_mode = mode;
}

power_mode_t get_current_power_mode(void) {
    return current_power_mode;
}

void enter_low_power_mode(void)
{
    /* Let Zephyr's power management handle the sleep */
    /* This will enter the appropriate low power state based on
     * what we've unlocked with pm_policy_state_lock_put() */
    
    if (current_power_mode != POWER_MODE_ACTIVE) {
        k_cpu_idle();  /* Enter lowest allowed power state */
    }
}

/* ========================================================================== */
/* POWER MANAGEMENT INITIALIZATION                                            */
/* ========================================================================== */
 
void power_management_init(void)
{
    int ret;
    LOG_INF("Initializing power management");
 
    /* Initialize system clock (Zephyr handles this) */
    init_system_clock();
 
    /* Initialize battery monitoring ADC */
    ret = battery_adc_init();
    if (ret != 0) {
        LOG_ERR("Battery ADC init failed: %d", ret);
        /* Continue anyway - non-critical */
    }
 
#ifdef CONFIG_USB_DETECT_PIN
    /* Initialize USB detect if available */
    ret = usb_detect_init();
    if (ret != 0) {
        LOG_WRN("USB detect init failed: %d (continuing without)", ret);
    }
#endif
 
    /* Start in active mode */
    current_power_mode = POWER_MODE_ACTIVE;
    set_power_mode(POWER_MODE_ACTIVE);
 
    /* Print initial battery status */
    uint16_t voltage = get_battery_voltage_mv();
    uint8_t percentage = get_battery_percentage();
    LOG_INF("Battery: %u mV (%u%%)", voltage, percentage);
 
    LOG_INF("Power management initialized");
}

/* ========================================================================== */
/* ADAPTIVE POWER MANAGEMENT                                                  */
/* ========================================================================== */
 
void power_optimize_for_battery_level(void)
{
    uint8_t battery_pct = get_battery_percentage();
 
    if (battery_pct < 5) {
        /* Critical - prepare for shutdown */
        LOG_WRN("Critical battery - entering sleep mode");
        set_power_mode(POWER_MODE_SLEEP);
    } else if (battery_pct < 10) {
        /* Low - use aggressive power saving */
        LOG_INF("Low battery - optimizing power");
        set_power_mode(POWER_MODE_SLEEP);
    } else if (battery_pct < 20) {
        /* Getting low - use moderate power saving */
        set_power_mode(POWER_MODE_IDLE);
    } else {
        /* Good battery - normal operation */
        set_power_mode(POWER_MODE_IDLE);
    }
}

void power_shutdown_gracefully(void)
{
    LOG_WRN("Initiating graceful shutdown");
 
    /* Give application time to save state, send BLE notifications, etc. */
    /* This should be called by the application before we force shutdown */
    k_msleep(500);  /* Brief delay for cleanup */
 
    /* Enter shutdown mode */
    set_power_mode(POWER_MODE_SHUTDOWN);
}

/* ========================================================================== */
/* DIAGNOSTICS AND STATUS                                                     */
/* ========================================================================== */
 
void power_print_status(void)
{
    uint16_t voltage = get_battery_voltage_mv();
    uint8_t percentage = get_battery_percentage();
    bool usb_conn = is_usb_connected();
    bool charging = is_charging();
 
    LOG_INF("=== Power Status ===");
    LOG_INF("Battery: %u mV (%u%%)", voltage, percentage);
    
    if (is_battery_critical()) {
        LOG_ERR("Battery: CRITICAL - Shutdown required!");
    } else if (is_battery_low()) {
        LOG_WRN("Battery: LOW - Charge soon");
    } else if (is_battery_full()) {
        LOG_INF("Battery: FULL");
    } else {
        LOG_INF("Battery: GOOD");
    }
 
    LOG_INF("USB: %s", usb_conn ? "Connected" : "Disconnected");
    LOG_INF("Charging: %s", charging ? "Yes" : "No");
    LOG_INF("Power Mode: %s", 
            current_power_mode == POWER_MODE_ACTIVE ? "ACTIVE" :
            current_power_mode == POWER_MODE_IDLE ? "IDLE" :
            current_power_mode == POWER_MODE_SLEEP ? "SLEEP" : "SHUTDOWN");
}

/* ========================================================================== */
/* WAKE SOURCE CONFIGURATION (RTOS handled)                                   */
/* ========================================================================== */
 
void enable_timer_wake(uint32_t ms)
{
    /* Zephyr handles timer-based wake automatically */
    /* Any pending timer (k_timer, k_work_schedule) will wake the system */
    LOG_DBG("Timer wake: %u ms", ms);
}
 
void enable_gpio_wake(uint8_t pin)
{
    /* Configure GPIO interrupt to wake from sleep */
    /* This is typically done when configuring the GPIO interrupt */
    LOG_DBG("GPIO wake enabled on pin %u", pin);
    
    /* On nRF52, GPIO interrupts can wake from System ON modes */
    /* For System OFF, need to configure as SENSE input */
}
 
void disable_all_wake_sources(void)
{
    /* Disable all GPIO interrupts */
    /* This would disable wake sources before shutdown */
    LOG_DBG("All wake sources disabled");
}

/* ========================================================================== */
/* CHARGING STATUS DETECTION                                                  */
/* ========================================================================== */
 
#ifdef CONFIG_USB_DETECT_PIN
#include <zephyr/drivers/gpio.h>
 
/* USB detect GPIO - configure in devicetree */
static const struct gpio_dt_spec usb_detect = 
    GPIO_DT_SPEC_GET(DT_ALIAS(usb_detect), gpios);
 
static int usb_detect_init(void)
{
    if (!device_is_ready(usb_detect.port)) {
        LOG_WRN("USB detect GPIO not available");
        return -ENODEV;
    }
 
    int ret = gpio_pin_configure_dt(&usb_detect, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Failed to configure USB detect pin: %d", ret);
        return ret;
    }
 
    return 0;
}
 
bool is_usb_connected(void)
{
#ifdef CONFIG_USB_DETECT_PIN
    return gpio_pin_get_dt(&usb_detect) == 1;
#else
    return false;
#endif
}
 
bool is_charging(void)
{
#ifdef CONFIG_USB_DETECT_PIN
    /* Charging if USB connected AND battery not full */
    if (!is_usb_connected()) {
        return false;
    }
    
    return !is_battery_full();
#else
    /* If no USB detect, can't determine charging status */
    return false;
#endif
}
 
#else
 
bool is_usb_connected(void) {
    return false;
}
 
bool is_charging(void) {
    return false;
}
 
#endif /* CONFIG_USB_DETECT_PIN */