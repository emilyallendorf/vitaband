/*
 * Power Management
 * Optimizes battery life for wearable device
 */

#ifndef POWER_H
#define POWER_H

#include <stdint.h>
#include <stdbool.h>

// Power modes
typedef enum {
    POWER_MODE_ACTIVE,      // Full power, all peripherals active
    POWER_MODE_IDLE,        // CPU sleep, peripherals active
    POWER_MODE_SLEEP,       // Deep sleep, wake on timer/interrupt
    POWER_MODE_SHUTDOWN     // Lowest power, wake on button only
} power_mode_t;

// System time functions
uint32_t get_system_time_ms(void);
void init_system_clock(void);

// Power management
void power_management_init(void);
void enter_low_power_mode(void);
void set_power_mode(power_mode_t mode);
power_mode_t get_current_power_mode(void);

// Battery monitoring
uint8_t get_battery_percentage(void);
uint16_t get_battery_voltage_mv(void);
bool is_battery_low(void);
bool is_charging(void);

// Wake sources
void enable_timer_wake(uint32_t ms);
void enable_gpio_wake(uint8_t pin);
void disable_all_wake_sources(void);

#endif // POWER_H
