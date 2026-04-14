/*
 * Minimal PCB bring-up: blink the board LED (devicetree alias led0).
 *
 * Pin assignment matches firmware/app.overlay: Alert LED on P0.06, active high.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(bringup, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)

#ifndef BLINK_INTERVAL_MS
#define BLINK_INTERVAL_MS 500
#endif

int main(void)
{
#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
	LOG_ERR("led0 is missing or disabled in devicetree — check app.overlay");
	return 0;
#else
	const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("LED GPIO device not ready");
		return 0;
	}

	int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (err != 0) {
		LOG_ERR("gpio_pin_configure_dt failed: %d", err);
		return 0;
	}

	LOG_INF("PCB bring-up: blinking led0 every %u ms", (unsigned int)BLINK_INTERVAL_MS);
	printk("VitaBand bring-up: toggle P0.%02u every %u ms\n",
	       (unsigned int)led.pin, (unsigned int)BLINK_INTERVAL_MS);

	bool on = false;
	for (;;) {
		on = !on;
		(void)gpio_pin_set_dt(&led, on);
		k_msleep(BLINK_INTERVAL_MS);
	}
#endif
}
