#ifndef TMP117_H
#define TMP117_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the TMP117 temperature sensor.
 * * Verifies the I2C bus is ready and configures the sensor.
 * * @return 0 on success, negative error code on failure.
 */
int tmp117_init(void);

/**
 * @brief Read the current temperature from the TMP117.
 * * @return Temperature value as uint8_t (or adjust to float/int16 for precision).
 */
float tmp117_read_temperature(void);

#endif /* TMP117_H */