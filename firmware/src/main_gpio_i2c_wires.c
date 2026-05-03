/*
 * Scope helpers on P0.26 / P0.27 (TWIM pins when I2C is disabled in overlay).
 *
 * Modes (Kconfig, see prj snippets):
 *   Default: slow opposite-phase toggle on P0.26 and P0.27 (~1 Hz).
 *   APP_GPIO_SCOPE_SCL_10K_P27=y: ~10 kHz square wave on **P0.27** (nRF52840 DK
 *     Arduino **SCL** = **D15**). Scope that pad for a “clock-like” trace.
 *   APP_GPIO_SCOPE_SCL_10K_P26=y: same on **P0.26** (e.g. VitaBand if SCL is routed there).
 *
 * west build ... -DCONF_FILE=prj_gpio_i2c_wires.conf -DDTC_OVERLAY_FILE=app_gpio_i2c_wires.overlay
 * 10 kHz on DK SCL pad:
 *   -DCONF_FILE=prj_gpio_scl_10k_dk.conf -DDTC_OVERLAY_FILE=app_gpio_i2c_wires.overlay
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define PORT    DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define PIN_26  26U
#define PIN_27  27U

/* ~10 kHz → 100 µs period, 50 µs half */
#define SCL_10K_HALF_US 50U

int main(void)
{
	int err;

	if (!gpio_is_ready(PORT)) {
		printk("gpio_wires: gpio0 not ready\n");
		return 0;
	}

#if IS_ENABLED(CONFIG_APP_GPIO_SCOPE_SCL_10K_P27)
	err = gpio_pin_configure(PORT, PIN_27, GPIO_OUTPUT);
	if (err != 0) {
		printk("gpio_wires: cfg P0.%u failed %d\n", PIN_27, err);
		return 0;
	}
	printk("\n=== GPIO scope: ~10 kHz on P0.%u (DK Arduino SCL / D15) ===\n", PIN_27);
	printk("gpio_wires: probe **D15** to GND; scope 0.1–1 ms/div\n");
	for (;;) {
		(void)gpio_pin_set(PORT, PIN_27, 1);
		k_usleep(SCL_10K_HALF_US);
		(void)gpio_pin_set(PORT, PIN_27, 0);
		k_usleep(SCL_10K_HALF_US);
	}
#elif IS_ENABLED(CONFIG_APP_GPIO_SCOPE_SCL_10K_P26)
	err = gpio_pin_configure(PORT, PIN_26, GPIO_OUTPUT);
	if (err != 0) {
		printk("gpio_wires: cfg P0.%u failed %d\n", PIN_26, err);
		return 0;
	}
	printk("\n=== GPIO scope: ~10 kHz on P0.%u (your SCL-on-26 pad) ===\n", PIN_26);
	printk("gpio_wires: scope 0.1–1 ms/div\n");
	for (;;) {
		(void)gpio_pin_set(PORT, PIN_26, 1);
		k_usleep(SCL_10K_HALF_US);
		(void)gpio_pin_set(PORT, PIN_26, 0);
		k_usleep(SCL_10K_HALF_US);
	}
#else
	err = gpio_pin_configure(PORT, PIN_26, GPIO_OUTPUT);
	if (err != 0) {
		printk("gpio_wires: cfg P0.%u failed %d\n", PIN_26, err);
		return 0;
	}
	err = gpio_pin_configure(PORT, PIN_27, GPIO_OUTPUT);
	if (err != 0) {
		printk("gpio_wires: cfg P0.%u failed %d\n", PIN_27, err);
		return 0;
	}

	printk("\n=== VitaBand GPIO_I2C_WIRE_PULSE (slow P0.26 / P0.27) ===\n");
	printk("gpio_wires: DK Arduino **D14=P0.26**, **D15=P0.27** — use **≥200 ms/div**\n");

	for (unsigned int tick = 0U;; tick++) {
		(void)gpio_pin_set(PORT, PIN_26, 1);
		(void)gpio_pin_set(PORT, PIN_27, 0);
		printk("gpio_wires: tick%u P0.%u=HI P0.%u=LO\n", tick, PIN_26, PIN_27);
		k_sleep(K_MSEC(500));
		(void)gpio_pin_set(PORT, PIN_26, 0);
		(void)gpio_pin_set(PORT, PIN_27, 1);
		printk("gpio_wires: tick%u P0.%u=LO P0.%u=HI\n", tick, PIN_26, PIN_27);
		k_sleep(K_MSEC(500));
	}
#endif
}
