#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <stdint.h>

typedef enum {
    OK,
    WARNING,
    CRITICAL,
    EMERGENCY
} vitaband_state_t;

/**
 * @brief Structure to hold the system's context
 */
typedef struct {
    vitaband_state_t current_state;
    uint8_t risk_score; // 1-10 scale
} vitaband_status_t;

/**
 * @brief Processes raw sensor data and returns a risk score (1-10)
 */
uint8_t calculate_risk_score(uint8_t heart_rate, float body_temp, float ambient_temp);

/**
 * @brief Determines the state based on the 1-10 risk score.
 */
vitaband_state_t determine_state(uint8_t risk_score);

#endif