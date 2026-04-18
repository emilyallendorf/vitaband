/*
 * TMP117-only I2C bring-up test (Segger RTT printk).
 * Same I2C0 pins as app.overlay (TWIM SCL=P0.26, SDA=P0.27).
 *
 * west build -b nrf52840dk/nrf52840 firmware -d firmware/build -p always -- \
 *   -DCONF_FILE=prj_tmp117_i2c.conf -DDTC_OVERLAY_FILE=app_tmp117_i2c.overlay
 * DK pin order: append ;app_tmp117_i2c_dkswap.overlay
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "tmp117.h"

#define SAMPLE_INTERVAL_MS 500U

#define TMP117_I2C_ADDR DT_REG_ADDR(DT_NODELABEL(tmp117))

static const uint8_t sht3x_soft_reset[] = { 0x30, 0xA2 };

static void tmp117_bus_diag(void)
{
	const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));

	if (!device_is_ready(i2c)) {
		printk("tmp117: diag i2c0 not ready\n");
		return;
	}

	printk("tmp117: diag base overlay pins: VitaBand SCL=P0.26 SDA=P0.27\n");
	printk("tmp117: diag on **DK Arduino only**, if every addr NACKs, rebuild with pin-swap fragment:\n");
	printk("tmp117: diag   -DDTC_OVERLAY_FILE=\"app_tmp117_i2c.overlay;app_tmp117_i2c_dkswap.overlay\"\n");

	(void)i2c_recover_bus(i2c);

	/* TMP117 device ID @ reg 0x0F → data 01 17 (TI); probe all ADD0 strap addresses */
	printk("tmp117: diag TMP117 ID @ reg 0x0F (ADD0→GND/V+/SDA/SCL = 0x48..0x4B):\n");
	for (uint16_t addr = 0x48; addr <= 0x4b; addr++) {
		uint8_t reg = 0x0f;
		uint8_t id[2];
		int r = i2c_write_read(i2c, addr, &reg, 1, id, sizeof(id));

		printk("tmp117:   @0x%02x -> %d", addr, r);
		if (r == 0) {
			printk(" id=%02x%02x\n", id[0], id[1]);
		} else {
			printk("\n");
		}
	}

	{
		int r44 = i2c_write(i2c, sht3x_soft_reset, sizeof(sht3x_soft_reset), 0x44);
		int r45 = i2c_write(i2c, sht3x_soft_reset, sizeof(sht3x_soft_reset), 0x45);

		printk("tmp117: diag SHT3x soft_reset: 0x44 -> %d, 0x45 -> %d (0=ACK)\n", r44,
		       r45);
	}

	printk("tmp117: diag summary: all -5 → no slave ACK on this bus (power/GND/wrong header row/\n");
	printk("tmp117:           pin order vs overlay, or sensor not populated). Firmware OK.\n");
}

int main(void)
{
	int ret;

	printk("\n=== VitaBand TMP117 I2C test ===\n");
	printk("tmp117: TMP117 I2C addr from devicetree reg = 0x%02x\n",
	       (unsigned int)TMP117_I2C_ADDR);
	printk("tmp117: (TMP117 ADD0: GND=0x48 V+=0x49 SDA=0x4A SCL=0x4B per datasheet)\n");

	k_sleep(K_MSEC(250));

	{
		const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));

		printk("tmp117: i2c0 ready: %s\n",
		       device_is_ready(i2c) ? "yes" : "no (TWIM/binding issue — check nordic,nrf-twim overlay)");
	}

	ret = tmp117_init();
	if (ret != 0) {
		printk("tmp117: init failed (%d)", ret);
		if (ret == -EIO) {
			printk(" (-EIO: no ACK at DT addr 0x%02x)\n", (unsigned int)TMP117_I2C_ADDR);
		} else {
			printk("\n");
		}
		tmp117_bus_diag();
		return 0;
	}

	printk("tmp117: OK, sampling every %u ms\n", SAMPLE_INTERVAL_MS);

	for (;;) {
		float t = tmp117_read_temperature();

		if (t < -90.0f) {
			printk("tmp117: read failed (sentinel temp)\n");
		} else {
			printk("tmp117: %.3f C\n", (double)t);
		}

		k_sleep(K_MSEC(SAMPLE_INTERVAL_MS));
	}
}
