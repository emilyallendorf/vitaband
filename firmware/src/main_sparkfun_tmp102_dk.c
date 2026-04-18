/*
 * SparkFun TMP102 on nRF52840 DK Arduino I2C — printk on Segger RTT.
 *
 * Build:
 *   i2c0: prj_sparkfun_tmp102_dk.conf + app_sparkfun_tmp102_dk.overlay
 *   i2c1 @ D14/D15: prj_sparkfun_tmp102_dk_i2c1.conf + app_sparkfun_tmp102_dk_i2c1.overlay
 *   i2c1 @ A4/A5:    prj_sparkfun_tmp102_dk_i2c1_a4a5.conf + app_sparkfun_tmp102_dk_i2c1_a4a5.overlay
 *   custom pins:   prj_sparkfun_tmp102_dk_custom_pins.conf + app_sparkfun_tmp102_dk_custom_pins.overlay
 * (D14/D15 = P0.26/P0.27; A4/A5 = P0.30/P0.31; custom overlay defaults P0.28/P0.29 — edit overlay.)
 *
 * If you see NACK / read errors: that is the TWIM reporting “no device answered
 * at this address on these pins” — almost always wiring, 3V3, GND, or SDA/SCL
 * swapped vs D14/D15, not a random application bug.
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define SAMPLE_MS 500U

#define TMP102_ADDR 0x48U

#if !DT_HAS_ALIAS(tmp102_i2c)
#error "Devicetree must define aliases.tmp102-i2c = &i2c0 or &i2c1 (see SparkFun DK overlays)."
#endif

#define TMP102_I2C_NODE DT_ALIAS(tmp102_i2c)

static int tmp102_raw_read(const struct device *i2c, float *out_c)
{
	uint8_t reg = 0x00;
	uint8_t buf[2];
	int ret = i2c_write_read(i2c, TMP102_ADDR, &reg, 1, buf, sizeof(buf));

	if (ret != 0) {
		return ret;
	}

	int16_t raw16 = (int16_t)(((uint16_t)buf[0] << 8U) | (uint16_t)buf[1]);

	*out_c = ((float)(raw16 >> 4)) * 0.0625f;
	return 0;
}

/* Returns 1 if any 7-bit address ACKed during scans, else 0. */
static int bus_diag(const struct device *i2c)
{
	static const uint8_t z = 0x00U;
	int rc;
	int any = 0;

	rc = i2c_recover_bus(i2c);
	printk("tmp102_dk: diag i2c_recover_bus -> %d", rc);
	if (rc == -ECANCELED) {
		printk(" (-ECANCELED: bus-recovery could not float SDA high — SDA short/GND, slave "
		       "holding bus, or SDA not on the pin in your overlay)\n");
	} else if (rc != 0) {
		printk("\n");
	} else {
		printk("\n");
	}

	{
		uint8_t rb[2];

		rc = i2c_write_read(i2c, TMP102_ADDR, &z, 1, rb, sizeof(rb));
		printk("tmp102_dk: diag write+read @0x48 (temp reg) -> %d", rc);
		if (rc == -EIO) {
			printk(" (-EIO: NACK at 0x48 — no powered I2C slave on this bus)\n");
		} else if (rc != 0) {
			printk("\n");
		} else {
			printk(" (OK)\n");
		}
	}

	printk("tmp102_dk: diag scan 0x40–0x4f (1-byte write 0x00):\n");
	for (uint16_t a = 0x40; a <= 0x4f; a++) {
		if (i2c_write(i2c, &z, 1, a) == 0) {
			printk("tmp102_dk:   ACK at 0x%02x\n", a);
			any = 1;
		}
	}
	if (any == 0) {
		printk("tmp102_dk:   (no ACK in 0x40–0x4f)\n");
		printk("tmp102_dk: widen scan 0x08–0x77 (few seconds)…\n");
		for (uint16_t a = 0x08; a <= 0x77; a++) {
			if (i2c_write(i2c, &z, 1, a) == 0) {
				printk("tmp102_dk:   ACK at 0x%02x\n", a);
				any = 1;
			}
		}
		if (any == 0) {
			printk("tmp102_dk:   (no ACK anywhere — not a TMP102 register bug; check D14/D15, "
			       "3V3, GND, cable, or a dead breakout)\n");
		}
	}

	return any;
}

int main(void)
{
	const struct device *i2c = DEVICE_DT_GET(TMP102_I2C_NODE);
	float t;
	int ret;

	k_sleep(K_MSEC(300));

	printk("\n=== SparkFun TMP102 (DK I2C, addr 0x48) ===\n");
	printk("tmp102_dk: bus device \"%s\" (aliases.tmp102-i2c)\n", i2c->name);

	if (!device_is_ready(i2c)) {
		printk("tmp102_dk: I2C \"%s\" not ready\n", i2c->name);
		return 0;
	}

	for (int ran_diag = 0;;) {
		ret = tmp102_raw_read(i2c, &t);

		if (ret == 0) {
			printk("tmp102_dk: %.2f C\n", (double)t);
		} else if (ran_diag == 0) {
			printk("tmp102_dk: read err %d", ret);
			if (ret == -EIO) {
				printk(" (I2C NACK — no ACK from slave)\n");
			} else {
				printk("\n");
			}
			const int any_ack = bus_diag(i2c);

			ran_diag = 1;
			if (ret == -EIO && any_ack == 0) {
				printk("\ntmp102_dk: HALT — TWIM saw zero ACKs (empty bus). Fix hardware, "
				       "then reset.\n");
				printk("tmp102_dk: 1) Meter: ~3.3 V between TMP102 VCC and GND (DK on USB).\n");
				printk("tmp102_dk: 2) Continuity: wire breakout **SDA/SCL** to the **exact** "
				       "MCU pins in your overlay (D14/D15, A4/A5, custom P0.xx, …).\n");
				printk("tmp102_dk: 3) Same GND for DK and breakout; TMP102 is 3.3 V I2C only.\n");
				printk("tmp102_dk: 4) Re-seat Qwiic / dupont; try another cable or breakout.\n");
				printk("tmp102_dk: 5) Confirm `west build -b nrf52840dk/nrf52840` matches "
				       "the chip silkscreen on your desk.\n");
				printk("tmp102_dk: 6) Optional: `<build>/zephyr/zephyr.dts` → node \"%s\" → "
				       "pinctrl `psels` match **your** SDA/SCL wires.\n",
				       i2c->name);
				for (;;) {
					k_sleep(K_FOREVER);
				}
			}
		} else {
			printk("tmp102_dk: read err %d\n", ret);
		}

		k_sleep(K_MSEC(SAMPLE_MS));
	}
}
