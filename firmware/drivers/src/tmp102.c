/*
 * TI TMP102 — minimal I2C reader (no devicetree child node required).
 * Uses I2C0 + 7-bit address 0x48 (SparkFun default ADD0 = GND).
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>

#include "tmp102.h"

#define NO_TEMP (-99.0f)

#define TMP102_I2C_ADDR 0x48U

static const struct device *const k_i2c0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));

int tmp102_init(void)
{
	if (!device_is_ready(k_i2c0)) {
		return -ENODEV;
	}
	return 0;
}

float tmp102_read_temperature(void)
{
	uint8_t reg = 0x00;
	uint8_t buf[2];
	int ret = i2c_write_read(k_i2c0, TMP102_I2C_ADDR, &reg, 1, buf, sizeof(buf));

	if (ret != 0) {
		return NO_TEMP;
	}

	int16_t raw16 = (int16_t)(((uint16_t)buf[0] << 8U) | (uint16_t)buf[1]);
	float celsius = ((float)(raw16 >> 4)) * 0.0625f;

	return celsius;
}
