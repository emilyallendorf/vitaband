/*
 * Minimal bring-up:
 *   MOTOR_EN P0.04 — GPIO high (enable).
 *   BUZZER_EN P0.05 — hardware PWM channel 0, ~4 kHz, 50% duty (piezo spec).
 *
 * P0.05 must not be configured as GPIO while pwm0 owns it (overlay routes PWM_OUT0).
 * printk uses Segger RTT (J-Link on the DK): nRF Connect “Connected Device” RTT,
 * or J-Link RTT Viewer — not the virtual COM port.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define HAPTIC_PORT DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define PWM_DEV     DEVICE_DT_GET(DT_NODELABEL(pwm0))

#define MOTOR_PIN 4U
#define BUZZER_PIN 5U
#define PWM_CH    0U

/* 4 kHz square-ish wave: period 250 µs, 50% duty */
#define BUZZ_PERIOD_NS 250000U
#define BUZZ_PULSE_NS  125000U

int main(void)
{
	int ret;

	printk("\n=== vitaband bring-up (printk) ===\n");

	if (!device_is_ready(HAPTIC_PORT)) {
		printk("ERROR: gpio0 not ready\n");
		return 0;
	}
	printk("OK: gpio0 ready\n");

	if (!device_is_ready(PWM_DEV)) {
		printk("ERROR: pwm0 not ready\n");
		return 0;
	}
	printk("OK: pwm0 ready\n");

	{
		uint64_t cps = 0U;

		ret = pwm_get_cycles_per_sec(PWM_DEV, PWM_CH, &cps);
		printk("pwm clock: ret=%d ~%u Hz (for cycle math)\n", ret,
		       (unsigned int)(cps > 0ULL ? cps : 0ULL));
	}

	ret = gpio_pin_configure(HAPTIC_PORT, MOTOR_PIN, GPIO_OUTPUT_HIGH);
	if (ret != 0) {
		printk("ERROR: motor GPIO configure failed: %d\n", ret);
		return 0;
	}
	printk("OK: P0.04 motor pin GPIO_OUTPUT_HIGH\n");

	// ret = gpio_pin_configure(HAPTIC_PORT, BUZZER_PIN, GPIO_OUTPUT_HIGH);
	// if (ret != 0) {
	// 	printk("ERROR: motor GPIO configure failed: %d\n", ret);
	// 	return 0;
	// }
	// printk("OK: P0.04 motor pin GPIO_OUTPUT_HIGH\n");

	// k_sleep(K_MSEC(5000)); /* wait for the motor to spin up before enabling the buzzer */

	ret = pwm_set(PWM_DEV, PWM_CH, BUZZ_PERIOD_NS, BUZZ_PULSE_NS, PWM_POLARITY_NORMAL);
	if (ret != 0) {
		printk("ERROR: pwm_set failed: %d (period_ns=%u pulse_ns=%u ch=%u)\n", ret,
		       BUZZ_PERIOD_NS, BUZZ_PULSE_NS, PWM_CH);
		return 0;
	}
	// printk("OK: pwm_set ch%u ~4 kHz (period %u ns, pulse %u ns)\n", PWM_CH,
	//        BUZZ_PERIOD_NS, BUZZ_PULSE_NS);
	// printk("Probe **MCU P0.05** (UART RTS on DK schematics) — not the green LED (**P0.13**).\n");
	// printk("Idle forever — PWM should run until reset.\n");

	for (;;) {
		k_sleep(K_FOREVER);
	}
}
