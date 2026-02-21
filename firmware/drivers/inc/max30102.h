/*
 * MAX30102 Heart Rate Sensor Driver
 * Header File
 */

#ifndef MAX30102_H
#define MAX30102_H

#include <zephyr/types.h>
#include <zephyr/types.h>
#include <stdbool.h>

/**
 * @brief Initialize the MAX30102 sensor
 * 
 * Performs the following:
 * - Checks I2C communication
 * - Verifies Part ID
 * - Resets the device
 * - Configures FIFO, PPG, LEDs
 * - Enables proximity detection
 * - Enables interrupts
 * 
 * @return 0 on success, negative errno on failure
 */
int max30102_init(void);

/**
 * @brief Enter normal measurement mode
 * 
 * Called when proximity is detected (skin contact).
 * Flushes FIFO and starts full-power data collection.
 */
void max30102_enter_normal_mode(void);

/**
 * @brief Enter low-power proximity detection mode
 * 
 * Used when there is no skin contact detected to save battery.
 */
void max30102_enter_proximity_mode(void);

/**
 * @brief Check if object is in proximity
 * 
 * @return true if proximity detected, false otherwise
 */
bool max30102_check_proximity(void);

/**
 * @brief Read samples from FIFO
 * 
 * @param sample_count Output: number of samples read
 * @param red_out Output buffer for red LED samples (can be NULL)
 * @param ir_out Output buffer for IR LED samples (can be NULL)
 * @return 0 on success, negative errno on failure
 */
int max30102_read_fifo(uint8_t *sample_count, int32_t *red_out, int32_t *ir_out);

/**
 * @brief Calculate heart rate from IR samples
 * 
 * Uses simple peak detection algorithm.
 * 
 * @param ir_samples Array of IR LED samples
 * @param num_samples Number of samples in array
 * @return Calculated heart rate in BPM (40-220 range)
 */
uint8_t max30102_calculate_heartrate(int32_t *ir_samples, uint8_t num_samples);

/**
 * @brief Read current heart rate
 * 
 * Convenience function that reads FIFO and calculates HR.
 * 
 * @return Heart rate in BPM, or last known value if no new data
 */
uint8_t max30102_read_heartrate(void);

#endif