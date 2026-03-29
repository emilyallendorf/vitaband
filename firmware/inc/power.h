/*
 * Power Management Module
 * Header File - Integrated Version
 */

#ifndef POWER_H
#define POWER_H

#include <zephyr/types.h>
#include <stdbool.h>
#include <config.h>

/* ========================================================================== */
/* POWER MODES                                                                */
/* ========================================================================== */

typedef enum {
    POWER_MODE_ACTIVE,      /* Full power, all peripherals active */
    POWER_MODE_IDLE,        /* CPU sleep, peripherals active (System ON Idle) */
    POWER_MODE_SLEEP,       /* Deep sleep, most peripherals off (System OFF with RAM retention) */
    POWER_MODE_SHUTDOWN     /* Lowest power, wake on button/reset only (System OFF) */
} power_mode_t;

/* ========================================================================== */
/* INITIALIZATION                                                             */
/* ========================================================================== */

/**
 * @brief Initialize power management subsystem
 * 
 * Initializes:
 * - System clock/timer
 * - Battery monitoring ADC
 * - USB detect (if available)
 * - Power mode control
 * 
 * Call this once during system initialization.
 */
void power_management_init(void);

/* ========================================================================== */
/* SYSTEM TIME                                                                */
/* ========================================================================== */

/**
 * @brief Initialize system clock
 * 
 * For Zephyr, this is handled automatically.
 * Kept for API compatibility.
 */
void init_system_clock(void);

/**
 * @brief Get system uptime
 * 
 * @return Milliseconds since boot
 */
uint32_t get_system_time_ms(void);

/* ========================================================================== */
/* BATTERY MONITORING                                                         */
/* ========================================================================== */

/**
 * @brief Read battery voltage
 * 
 * Reads ADC and converts to battery voltage in millivolts.
 * Accounts for voltage divider if present.
 * 
 * @return Battery voltage in mV (3000-4200 typical)
 */
uint16_t get_battery_voltage_mv(void);

/**
 * @brief Get battery percentage
 * 
 * Uses LiPo discharge curve to estimate remaining capacity.
 * 
 * @return Battery percentage (0-100)
 */
uint8_t get_battery_percentage(void);

/**
 * @brief Check if battery is low
 * 
 * Warning threshold - user should charge soon.
 * 
 * @return true if battery < 10% (< 3.3V)
 */
bool is_battery_low(void);

/**
 * @brief Check if battery is critical
 * 
 * Emergency threshold - device should shutdown.
 * 
 * @return true if battery < 0% (< 3.0V)
 */
bool is_battery_critical(void);

/**
 * @brief Check if battery is full
 * 
 * @return true if battery >= 100% (>= 4.2V)
 */
bool is_battery_full(void);

/* ========================================================================== */
/* CHARGING STATUS (if USB detect available)                                 */
/* ========================================================================== */

/**
 * @brief Check if USB is connected
 * 
 * Requires USB detect pin configured in devicetree.
 * 
 * @return true if USB power detected
 */
bool is_usb_connected(void);

/**
 * @brief Check if battery is charging
 * 
 * Requires USB detect pin configured in devicetree.
 * 
 * @return true if USB connected and battery not full
 */
bool is_charging(void);

/* ========================================================================== */
/* POWER MODE CONTROL                                                         */
/* ========================================================================== */

/**
 * @brief Set power mode
 * 
 * Changes system power state:
 * - ACTIVE: No sleep, all peripherals on
 * - IDLE: CPU sleep between events, peripherals active
 * - SLEEP: Deep sleep, most peripherals off
 * - SHUTDOWN: System OFF, wake on button/reset only
 * 
 * @param mode Desired power mode
 */
void set_power_mode(power_mode_t mode);

/**
 * @brief Get current power mode
 * 
 * @return Current power mode
 */
power_mode_t get_current_power_mode(void);

/**
 * @brief Enter low power mode
 * 
 * Puts CPU to sleep based on current power mode setting.
 * Will wake on next interrupt (timer, BLE, GPIO, etc.)
 * 
 * Call this in your main loop idle periods.
 */
void enter_low_power_mode(void);

/* ========================================================================== */
/* WAKE SOURCE CONFIGURATION                                                  */
/* ========================================================================== */

/**
 * @brief Enable timer-based wake
 * 
 * Configure timer to wake system after specified time.
 * For Zephyr, any pending k_timer will wake automatically.
 * 
 * @param ms Milliseconds until wake
 */
void enable_timer_wake(uint32_t ms);

/**
 * @brief Enable GPIO wake
 * 
 * Configure GPIO pin as wake source.
 * 
 * @param pin GPIO pin number
 */
void enable_gpio_wake(uint8_t pin);

/**
 * @brief Disable all wake sources
 * 
 * Disables all interrupts that can wake the system.
 * Use before entering shutdown mode.
 */
void disable_all_wake_sources(void);

/* ========================================================================== */
/* DIAGNOSTICS                                                                */
/* ========================================================================== */

/**
 * @brief Print power status
 * 
 * Logs:
 * - Battery voltage and percentage
 * - Battery status (FULL/GOOD/LOW/CRITICAL)
 * - USB connection status
 * - Charging status
 * - Current power mode
 */
void power_print_status(void);

/* ========================================================================== */
/* ADAPTIVE POWER MANAGEMENT                                                  */
/* ========================================================================== */

/**
 * @brief Optimize power mode based on battery level
 * 
 * Automatically adjusts power mode:
 * - Battery > 20%: IDLE mode
 * - Battery 10-20%: IDLE mode
 * - Battery 5-10%: SLEEP mode (aggressive saving)
 * - Battery < 5%: SLEEP mode (prepare for shutdown)
 * 
 * Call periodically (e.g., every minute) to adapt to battery level.
 */
void power_optimize_for_battery_level(void);

/**
 * @brief Gracefully shutdown the system
 * 
 * Gives application time to:
 * - Save state
 * - Send final BLE notifications
 * - Turn off peripherals
 * 
 * Then enters SHUTDOWN mode.
 */
void power_shutdown_gracefully(void);

#endif /* POWER_H */
