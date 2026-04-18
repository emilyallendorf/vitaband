/*
 * Ambient SHT3x-DIS: periodic temperature + humidity (Segger RTT printk) via Zephyr SHT3XD driver.
 *
 * Build: prj_ambient_pwm.conf + app_ambient_pwm.overlay
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* Same command as Zephyr sht3xd.c init: SHT3XD_CMD_CLEAR_STATUS = 0x3041 */
static const uint8_t sht3xd_clear_status[] = { 0x30, 0x41 };

int main(void)
{
	const struct device *sht = DEVICE_DT_GET(DT_NODELABEL(sht3xdis));
	const struct device *bus = DEVICE_DT_GET(DT_PARENT(DT_NODELABEL(sht3xdis)));

	k_sleep(K_MSEC(2000)); /* wait for RTT viewer to attach */

	printk("\n=== VitaBand ambient SHT3x-DIS demo ===\n");

	/* #region agent log */
	printk("ambient/dbg: hyp=A bus=%s parent_ready=%d\n", bus->name,
	       device_is_ready(bus));
	{
		int p44 = i2c_write(bus, sht3xd_clear_status, sizeof(sht3xd_clear_status),
				    0x44);
		int p45 = i2c_write(bus, sht3xd_clear_status, sizeof(sht3xd_clear_status),
				    0x45);

		printk("ambient/dbg: hyp=B I2C clear-status write: 0x44->%d 0x45->%d "
		       "(0=ACK; -EIO typical NACK)\n",
		       p44, p45);
	}
	/* #endregion */

	if (!device_is_ready(sht)) {
		printk("SHT3x not ready — power, solder, or I2C pinmux vs PCB (shared bus with TMP117)\n");
		printk("hint: app_ambient_pwm.overlay must match VitaBand SCL/SDA pads; optional "
		       "app_ambient_pwm_i2c_dkswap.overlay for legacy pin order.\n");
		printk("hint: if only 0x44 ACKs, set reg = <0x44> (ADDR pin to GND).\n");
		return -1;
	}

	printk("SHT3x ready, sampling every 500ms\n");

	for (;;) {
		struct sensor_value temp, humidity;

		sensor_sample_fetch(sht);
		sensor_channel_get(sht, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		sensor_channel_get(sht, SENSOR_CHAN_HUMIDITY, &humidity);

		printk("Temp: %d.%06d C  Humidity: %d.%06d %%RH\n",
		       temp.val1, temp.val2,
		       humidity.val1, humidity.val2);

		k_sleep(K_MSEC(500));
	}
}
