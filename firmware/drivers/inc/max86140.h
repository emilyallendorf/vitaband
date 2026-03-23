/*
 * MAX86140 Heart Rate / PPG Sensor Driver
 * Header File
 */

#ifndef MAX86140_H_
#define MAX86140_H_

#include <zephyr/types.h>

/**
 * @brief Initialize MAX86140 sensor
 * 
 * Initializes SPI communication, verifies Part ID, and configures sensor.
 * 
 * @return 0 on success, negative errno on failure
 */
int max86140_init(void);

/**
 * @brief Read heart rate
 * 
 * NOTE: This is currently a placeholder!
 * Full implementation requires:
 * - FIFO reading
 * - Signal processing
 * - Peak detection algorithm
 * 
 * @return Heart rate in BPM (placeholder value for now)
 */
uint8_t max86140_read_heartrate(void);

/**
 * @brief Print sensor status
 * 
 * Logs Part ID and FIFO status for debugging.
 */
void max86140_print_status(void);

#endif /* MAX86140_H_ */