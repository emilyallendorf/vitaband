#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include "sht3x-dis.h"

LOG_MODULE_REGISTER(sht3xdis, LOG_LEVEL_INF);

/* Single Shot Data Acquisition (High Repeatability, clock stretching) */
#define SHT3X_CMD_MEAS_CLOCKSTR_H  0x2C06
#define SHT3X_CMD_SOFT_RESET       0x30A2

#define NO_TEMP -99.0f
#define NO_HUM    0.0f

static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(sht3xdis));

// static int sht3xdis_send_cmd(uint16_t cmd)
// {
// 	uint8_t cmd_buf[2];

// 	cmd_buf[0] = (uint8_t)((cmd >> 8) & 0xFF);
// 	cmd_buf[1] = (uint8_t)(cmd & 0xFF);
// 	return i2c_write_dt(&i2c_dev, cmd_buf, sizeof(cmd_buf));
// }

static const struct device *i2c_bus = DEVICE_DT_GET(DT_NODELABEL(i2c0));

static int sht3xdis_send_cmd(uint16_t cmd)
{
    uint8_t cmd_buf[2];
    cmd_buf[0] = (uint8_t)((cmd >> 8) & 0xFF);
    cmd_buf[1] = (uint8_t)(cmd & 0xFF);
    return i2c_write(i2c_bus, cmd_buf, sizeof(cmd_buf), 0x45);
}

static int sht3xdis_read_data(uint8_t *buffer, size_t len)
{
	return i2c_read_dt(&i2c_dev, buffer, len);
}

int sht3xdis_init(void)
{

	printk("SHT3x I2C addr: 0x%02x\n", i2c_dev.addr);
	printk("SHT3x bus ready: %d\n", device_is_ready(i2c_dev.bus));
	printk("SHT3x bus name: %s\n", i2c_dev.bus->name);

	if (!device_is_ready(i2c_dev.bus)) {
		printk("I2C bus not ready for SHT3x\n");
		return -ENODEV;
	}

	  k_msleep(2000);

	int ret = sht3xdis_send_cmd(SHT3X_CMD_SOFT_RESET);
	if (ret != 0) {
		printk("SHT3x soft reset failed: %d\n", ret);
		return ret;
	}
	k_msleep(2);

	LOG_INF("SHT3x-DIS initialized");
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
