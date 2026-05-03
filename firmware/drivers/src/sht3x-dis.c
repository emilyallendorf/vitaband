#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include "sht3x-dis.h"

LOG_MODULE_REGISTER(sht3xdis, LOG_LEVEL_DBG);

/* Single Shot Data Acquisition (High Repeatability, clock stretching) */
#define SHT3X_CMD_MEAS_CLOCKSTR_H  0x2C06
#define SHT3X_CMD_SOFT_RESET       0x30A2

#define NO_TEMP -99.0f
#define NO_HUM    0.0f

static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(sht3xdis));

static int sht3xdis_send_cmd(uint16_t cmd)
{
	uint8_t cmd_buf[2];

	cmd_buf[0] = (uint8_t)((cmd >> 8) & 0xFF);
	cmd_buf[1] = (uint8_t)(cmd & 0xFF);
	return i2c_write_dt(&i2c_dev, cmd_buf, sizeof(cmd_buf));
}

static int sht3xdis_read_data(uint8_t *buffer, size_t len)
{
	return i2c_read_dt(&i2c_dev, buffer, len);
}

int sht3xdis_init(void)
{
	if (!device_is_ready(i2c_dev.bus)) {
		LOG_ERR("I2C bus not ready for SHT3x");
		return -ENODEV;
	}

	k_msleep(2000);

	int ret = sht3xdis_send_cmd(SHT3X_CMD_SOFT_RESET);
	if (ret != 0) {
		LOG_ERR("SHT3x soft reset failed: %d", ret);
		return ret;
	}
	k_msleep(2);

	LOG_DBG("SHT3x-DIS initialized");
	return 0;
}

int sht3xdis_read_all(float *temp_c, float *humidity)
{
	uint8_t data[6];

	int ret = sht3xdis_send_cmd(SHT3X_CMD_MEAS_CLOCKSTR_H);
	if (ret != 0) {
		return ret;
	}
	k_msleep(20);

	ret = sht3xdis_read_data(data, sizeof(data));
	if (ret != 0) {
		return ret;
	}

	uint16_t raw_t = ((uint16_t)data[0] << 8) | data[1];
	*temp_c = -45.0f + 175.0f * ((float)raw_t / 65535.0f);

	uint16_t raw_h = ((uint16_t)data[3] << 8) | data[4];
	*humidity = 100.0f * ((float)raw_h / 65535.0f);

	return 0;
}

float sht3xdis_read_temperature(void)
{
	float t, h;

	if (sht3xdis_read_all(&t, &h) == 0) {
		return t;
	}
	return NO_TEMP;
}

uint8_t sht3xdis_read_humidity(void)
{
	float t, h;

	if (sht3xdis_read_all(&t, &h) == 0) {
		return (uint8_t)h;
	}
	return NO_HUM;
}
