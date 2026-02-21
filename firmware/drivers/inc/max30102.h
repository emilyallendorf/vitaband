#ifndef MAX30102_H
#define MAX30102_H

#include <zephyr/types.h>

/** @brief Initialize the MAX30102 sensor */
int max30102_init(void);

/** @brief Read the latest heart rate value */
uint8_t max30102_read_heartrate(void);

#endif