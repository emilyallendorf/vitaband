/*
 * nRF52840 on-chip die temperature (no external I2C).
 * Verifies RTT + sensor driver path independent of SCL/SDA wiring.
 *
 * west build -b nrf52840dk/nrf52840 firmware -d build-onchip-temp -p always -- \
 *   -DCONF_FILE=prj_onchip_temp.conf
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define TEMP_DEV DEVICE_DT_GET(DT_NODELABEL(temp))

int main(void)
{
	struct sensor_value val;

	if (!device_is_ready(TEMP_DEV)) {
		printk("onchip: temp device not ready\n");
		return 0;
	}

	printk("\n=== VitaBand nRF52840 on-chip die temp ===\n");

	for (;;) {
		int ret = sensor_sample_fetch(TEMP_DEV);

		if (ret != 0) {
			printk("onchip: sensor_sample_fetch -> %d\n", ret);
		} else {
			ret = sensor_channel_get(TEMP_DEV, SENSOR_CHAN_DIE_TEMP, &val);
			if (ret != 0) {
				printk("onchip: sensor_channel_get -> %d\n", ret);
			} else {
				printk("onchip: die temp ~ %.2f C\n", sensor_value_to_double(&val));
			}
		}

		k_sleep(K_SECONDS(1));
	}
}
