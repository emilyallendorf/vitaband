/*
 * Minimal power helpers — battery voltage/percentage removed (no valid sense input).
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/policy.h>
#include <zephyr/logging/log.h>

#include "power.h"

LOG_MODULE_REGISTER(power, LOG_LEVEL_INF);

static power_mode_t current_power_mode = POWER_MODE_ACTIVE;

void init_system_clock(void)
{
	/* Zephyr uptime is always available. */
}

uint32_t get_system_time_ms(void)
{
	return k_uptime_get_32();
}

void set_power_mode(power_mode_t mode)
{
	switch (mode) {
	case POWER_MODE_ACTIVE:
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
		break;
	case POWER_MODE_IDLE:
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
		break;
	case POWER_MODE_SLEEP:
		pm_policy_state_lock_put(PM_STATE_SUSPEND_TO_IDLE, PM_ALL_SUBSTATES);
		pm_policy_state_lock_put(PM_STATE_STANDBY, PM_ALL_SUBSTATES);
		pm_policy_state_lock_get(PM_STATE_SUSPEND_TO_RAM, PM_ALL_SUBSTATES);
		break;
	case POWER_MODE_SHUTDOWN:
		current_power_mode = POWER_MODE_SHUTDOWN;
		LOG_INF("Shutdown: halt (no battery rail control on this board)");
		k_sleep(K_FOREVER);
		return;
	}

	current_power_mode = mode;
}

power_mode_t get_current_power_mode(void)
{
	return current_power_mode;
}

void enter_low_power_mode(void)
{
	if (current_power_mode != POWER_MODE_ACTIVE) {
		k_cpu_idle();
	}
}

void power_shutdown_gracefully(void)
{
	k_msleep(200);
	set_power_mode(POWER_MODE_SHUTDOWN);
}

void power_management_init(void)
{
	init_system_clock();
	current_power_mode = POWER_MODE_ACTIVE;
	set_power_mode(POWER_MODE_ACTIVE);
	LOG_INF("Power init (no battery monitoring)");
}
