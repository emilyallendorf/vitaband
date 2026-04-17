/*
 * TI TMP102 (SparkFun breakout default I2C address 0x48).
 */

#ifndef TMP102_H_
#define TMP102_H_

#include <stdint.h>

/**
 * @return 0 on success, negative errno on failure.
 */
int tmp102_init(void);

/**
 * @return Temperature in °C, or -99.0f on read error.
 */
float tmp102_read_temperature(void);

#endif
