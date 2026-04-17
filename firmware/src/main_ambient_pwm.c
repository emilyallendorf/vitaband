/*
 * Ambient SHT3x-DIS: periodic temperature + humidity (Segger RTT printk).
 *
 * Build: prj_ambient_pwm.conf + app_ambient_pwm.overlay
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "sht3x-dis.h"

#define SAMPLE_INTERVAL_MS 500U

int main(void)
{
	int ret;

	printk("\n=== VitaBand ambient SHT3x-DIS demo ===\n");
	k_sleep(K_MSEC(500));

	ret = sht3xdis_init();
	if (ret != 0) {
		printk("ambient: SHT3x init failed (%d)\n", ret);
		return 0;
	}

	printk("ambient: SHT3x OK, sampling every %u ms\n", SAMPLE_INTERVAL_MS);

	for (;;) {
		float temp_c;
		float rh_pct;

		ret = sht3xdis_read_all(&temp_c, &rh_pct);
		if (ret != 0) {
			printk("ambient: read failed (%d)\n", ret);
		} else {
			printk("ambient: %.2f C, %.1f %%RH\n", (double)temp_c, (double)rh_pct);
		}

		k_sleep(K_MSEC(SAMPLE_INTERVAL_MS));
	}
}
