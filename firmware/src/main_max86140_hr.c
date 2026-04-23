/*
 * Isolated MAX86140/MAX86141 heart-rate smoke test (SPI + drivers/src/max86140.c).
 *
 * Pick **one** overlay to match wiring (wrong overlay → Part ID 0x00):
 *   - VitaBand PCB / SPI0 device P0.x: app_max86140_hr_vitaband.overlay
 *     (RTT shows spi@40003000 = SPI0.)
 *   - nRF52840 DK + Maxim on Arduino SPI header: app_max86140_hr.overlay (SPI3).
 *
 * Build: prj_max86140_hr.conf + -DDTC_OVERLAY_FILE=<chosen overlay>.
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "max86140.h"

/* Longer period → more IR samples per FIFO read so peak detection has n≥3–10 frames. */
#define SAMPLE_PERIOD_MS 220U
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
		if (ret == -EIO) {
			printk("\nmax86140_init failed: Part ID read as 0x00 (-EIO).\n"
			       "If the sensor is on the DK Arduino SPI pins, build with "
			       "app_max86140_hr.overlay (SPI3), NOT app_max86140_hr_vitaband "
			       "(SPI0 P0.13–P0.16).\n"
			       "Check 3V3/GND, CS/SCK/MOSI/MISO, and that RTT showed "
			       "spi@40003000 for VitaBand overlay.\n\n");
		} else {
			printk("max86140_init failed: %d — check SPI wiring, CS, 3V3, overlay\n",
			       ret);
		}
		return ret;
	}

	printk("Sensor started. Cover LED/PD with finger for PPG; BPM updates when peaks detected.\n");

	static uint8_t show_bpm;

	for (;;) {
		uint8_t raw = max86140_read_heartrate();

		if (raw > 0U) {
			if (show_bpm == 0U) {
				show_bpm = raw;
			} else {
				show_bpm = (uint8_t)(((uint16_t)show_bpm * 3U + (uint16_t)raw + 2U) / 4U);
			}
		} else {
			show_bpm = 0U;
		}

		uint32_t now = k_uptime_get_32();
		if (now - last_print >= PRINT_INTERVAL_MS) {
			last_print = now;
			printk("HR (estimate): %u BPM\n", show_bpm);
		}

		k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
	}
}
