/*
 * Drive PWM on P0.26 and P0.27 (pwm0 channels 1 and 2) for scope / probe.
 *
 * Build: prj_pwm_i2c_pins_scope.conf + app_pwm_i2c_pins_scope.overlay
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define PWM_DEV DEVICE_DT_GET(DT_NODELABEL(pwm0))

/* Channel ids match PWM_OUTn in overlay (OUT1=P0.26, OUT2=P0.27). */
#define CH_P26 1U
#define CH_P27 2U

/* ~10 kHz */
#define PERIOD_NS 100000U

int main(void)
{
	int ret;

	k_sleep(K_MSEC(500));

	printk("\n=== VitaBand PWM scope: P0.26 (ch%u) + P0.27 (ch%u) @ ~10 kHz ===\n",
	       CH_P26, CH_P27);

	if (!device_is_ready(PWM_DEV)) {
		printk("pwm0 not ready\n");
		return -1;
	}

	/* P0.26: 50 % duty; P0.27: 25 % duty — easy to tell apart on a scope. */
	ret = pwm_set(PWM_DEV, CH_P26, PERIOD_NS, PERIOD_NS / 2U, PWM_POLARITY_NORMAL);
	if (ret != 0) {
		printk("pwm_set ch%u failed: %d\n", CH_P26, ret);
	}

	ret = pwm_set(PWM_DEV, CH_P27, PERIOD_NS, PERIOD_NS / 4U, PWM_POLARITY_NORMAL);
	if (ret != 0) {
		printk("pwm_set ch%u failed: %d\n", CH_P27, ret);
	}

	if (ret == 0) {
		printk("Running — probe D14=P0.26, D15=P0.27 (DK) or VitaBand I²C SDA/SCL pads.\n");
	}

	for (;;) {
		k_sleep(K_SECONDS(3600));
	}
}
