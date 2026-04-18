/*
 * Ambient SHT3x-DIS: periodic temperature + humidity (Segger RTT printk).
 *
 * Build: prj_ambient_pwm.conf + app_ambient_pwm.overlay
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "sht3x-dis.h"

#define SAMPLE_INTERVAL_MS 500U

#define SHT3X_I2C_ADDR DT_REG_ADDR(DT_NODELABEL(sht3xdis))

/* Same as SHT3X_CMD_SOFT_RESET in drivers/src/sht3x-dis.c */
static const uint8_t sht3x_soft_reset[] = { 0x30, 0xA2 };

static void ambient_print_init_err(int err)
{
	printk("ambient: SHT3x init failed (%d)", err);
	if (err == -EIO) {
		printk(" (-EIO: no I2C ACK — DT reg vs ADDR pin (GND=0x44, VDD=0x45), ");
		printk("pins, power, pull-ups)\n");
	} else if (err == -ENODEV) {
		printk(" (-ENODEV: I2C controller not ready)\n");
	} else {
		printk("\n");
	}
}

static void ambient_i2c_bus_diag(void)
{
	const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	static const uint8_t byte0[] = { 0 };
	int any = 0;
	int rc;

	if (!device_is_ready(i2c)) {
		printk("ambient: diag i2c0 not ready\n");
		return;
	}

	rc = i2c_recover_bus(i2c);
	printk("ambient: diag i2c_recover_bus -> %d\n", rc);

	printk("ambient: diag scan 0x40-0x4f (1-byte write 0x00, expect hits if bus OK):\n");
	for (uint16_t a = 0x40; a <= 0x4f; a++) {
		if (i2c_write(i2c, byte0, 1, a) == 0) {
			printk("ambient:   ACK at 0x%02x\n", a);
			any = 1;
		}
	}
	if (!any) {
		printk("ambient:   (no ACK — likely wrong SCL/SDA pins or no slaves / no pull-ups)\n");
	}

	{
		int r44 = i2c_write(i2c, sht3x_soft_reset, sizeof(sht3x_soft_reset), 0x44);
		int r45 = i2c_write(i2c, sht3x_soft_reset, sizeof(sht3x_soft_reset), 0x45);

		printk("ambient: diag SHT3x soft_reset: 0x44 -> %d, 0x45 -> %d (0 = ACK)\n", r44,
		       r45);
	}
}

int main(void)
{
	int ret;

	printk("\n=== VitaBand ambient SHT3x-DIS demo ===\n");
	printk("ambient: build sees sht3xdis I2C addr 0x%02x (expect 0x45 if ADDR→VDD)\n",
	       (unsigned int)SHT3X_I2C_ADDR);
	printk("ambient: TWIM pinctrl in overlay: SCL=P0.26 SDA=P0.27 (use *_i2c_dkswap if swapped)\n");
	k_sleep(K_MSEC(100));

	const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	printk("i2c0 ready: %d\n", device_is_ready(i2c));

	ret = sht3xdis_init();
	if (ret != 0) {
		printk("error: %d\n", ret);
		ambient_i2c_bus_diag();
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
