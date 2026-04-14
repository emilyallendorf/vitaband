#ifndef SHT3XDIS_H
#define SHT3XDIS_H

#include <stdint.h>
#include <stdbool.h>
#include <config.h>

/**
 * @brief Initialize the SHT3x-DIS ambient sensor.
 * * Checks for I2C bus readiness and prepares the sensor for
 * single-shot or periodic measurement mode.
 * * @return 0 on success, negative error code on failure.
 */
int sht3xdis_init(void);

/**
 * @brief Single-shot temperature + relative humidity (one I2C measurement).
 * @param temp_c Output temperature in °C.
 * @param rh_pct Output relative humidity in % (0–100).
 * @return 0 on success, negative errno from I2C layer on failure.
 */
int sht3xdis_read_all(float *temp_c, float *rh_pct);

/**
 * @brief Read the ambient temperature.
 * * @return Temperature as uint8_t (or float/int16_t for high precision).
 */
float sht3xdis_read_temperature(void);

/**
 * @brief Read the ambient humidity.
 * * @return Relative humidity percentage.
 */
uint8_t sht3xdis_read_humidity(void);

#endif /* SHT3XDIS_H */