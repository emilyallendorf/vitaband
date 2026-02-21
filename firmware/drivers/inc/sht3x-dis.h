#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the SHT3x-DIS ambient sensor.
 * * Checks for I2C bus readiness and prepares the sensor for
 * single-shot or periodic measurement mode.
 * * @return 0 on success, negative error code on failure.
 */
int uint8_t sht3xdis_init(void);

/**
 * @brief Read the ambient temperature.
 * * @return Temperature as uint8_t (or float/int16_t for high precision).
 */
uint8_t sht3xdis_read_temperature(void);

/**
 * @brief Read the ambient humidity.
 * * @return Relative humidity percentage.
 */
uint8_t sht3xdis_read_humidity(void);

#endif /* SHT3XDIS_H */