/*
 * Isolated MAX86140/MAX86141 heart-rate smoke test (SPI + drivers/src/max86140.c).
 *
 * Build: prj_max86140_hr.conf + app_max86140_hr.overlay
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "max86140.h"

#define SAMPLE_PERIOD_MS 100U
#define PRINT_INTERVAL_MS 1000U

int main(void)
{
	int ret;
	uint32_t last_print = 0U;

	/* Immediate line proves main() ran; open RTT *before* reset if you miss this. */
	printk("vitaband_hr: boot OK\n");

	/* Same idea as ambient demo: attach RTT Viewer, then reset so you do not miss banners. */
	k_sleep(K_MSEC(2000));

	printk("\n=== VitaBand MAX86140 HR demo (SPI) ===\n");

	ret = max86140_init();
	if (ret != 0) {
		printk("max86140_init failed: %d — check SPI wiring, CS, 3V3, overlay\n", ret);
		return ret;
	}

	printk("Sensor started. Cover LED/PD with finger for PPG; BPM updates when peaks detected.\n");

	for (;;) {
		uint8_t bpm = max86140_read_heartrate();

		uint32_t now = k_uptime_get_32();
		if (now - last_print >= PRINT_INTERVAL_MS) {
			last_print = now;
			printk("HR (estimate): %u BPM\n", bpm);
		}

		k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
	}
}
