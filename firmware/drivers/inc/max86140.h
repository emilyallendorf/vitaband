#ifndef MAX86140_H
#define MAX86140_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the MAX86140 via SPI.
 * @return 0 on success, negative errno on failure.
 */
int max86140_init(void);

/**
 * @brief Read heart rate from the most recent FIFO samples.
 * @return Heart rate in BPM, or last known value if no new peaks detected.
 */
uint8_t max86140_read_heartrate(void);

#endif /* MAX86140_H */