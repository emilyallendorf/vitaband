/*
 * TMP102 (SparkFun / TI) on I2C → PWM duty on P0.05 for scope (no UART on PCB).
 * Use on nRF52840 DK to verify I2C before VitaBand ambient SHT3x.
 *
 * Build: prj_tmp102_pwm.conf + app_tmp102_pwm.overlay
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <stdint.h>

#include "tmp102.h"

#define PWM_DEV DEVICE_DT_GET(DT_NODELABEL(pwm0))
#define PWM_CH  0U

#define PWM_PERIOD_NS 100000U

/* TMP102: about −55 °C … +150 °C usable; map linearly for scope duty */
#define TEMP_MAP_MIN_C (-55.0f)
#define TEMP_MAP_MAX_C (150.0f)

#define SAMPLE_INTERVAL_MS 500U

static float temp_to_duty_ratio(float temp_c)
{
	if (temp_c < -90.0f) {
		return 0.0f;
	}

	float t = temp_c;
	if (t < TEMP_MAP_MIN_C) {
		t = TEMP_MAP_MIN_C;
	}
	if (t > TEMP_MAP_MAX_C) {
		t = TEMP_MAP_MAX_C;
	}

	return (t - TEMP_MAP_MIN_C) / (TEMP_MAP_MAX_C - TEMP_MAP_MIN_C);
}

int main(void)
{
	int ret;

	if (!device_is_ready(PWM_DEV)) {
		return 0;
	}

	ret = tmp102_init();
	if (ret != 0) {
		(void)pwm_set(PWM_DEV, PWM_CH, PWM_PERIOD_NS, PWM_PERIOD_NS / 2U,
			      PWM_POLARITY_NORMAL);
		for (;;) {
			k_sleep(K_FOREVER);
		}
	}

	for (;;) {
		float temp_c = tmp102_read_temperature();
		float ratio = temp_to_duty_ratio(temp_c);
		uint32_t pulse_ns = (uint32_t)((float)PWM_PERIOD_NS * ratio + 0.5f);

		if (pulse_ns > PWM_PERIOD_NS) {
			pulse_ns = (uint32_t)PWM_PERIOD_NS;
		}

		ret = pwm_set(PWM_DEV, PWM_CH, PWM_PERIOD_NS, pulse_ns, PWM_POLARITY_NORMAL);
		(void)ret;

		k_sleep(K_MSEC(SAMPLE_INTERVAL_MS));
	}
}
