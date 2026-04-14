/*
 * Power helpers — no battery fuel gauge on this hardware (no usable Vbat ADC).
 */

#ifndef POWER_H
#define POWER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	POWER_MODE_ACTIVE,
	POWER_MODE_IDLE,
	POWER_MODE_SLEEP,
	POWER_MODE_SHUTDOWN
} power_mode_t;

void power_management_init(void);

void init_system_clock(void);
uint32_t get_system_time_ms(void);

void set_power_mode(power_mode_t mode);
power_mode_t get_current_power_mode(void);
void enter_low_power_mode(void);

void power_shutdown_gracefully(void);

#endif /* POWER_H */
