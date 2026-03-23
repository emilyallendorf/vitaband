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

void state_manager_init(void);

// const char* get_state_string(enum device_state state);


void handle_state_transition(vitaband_state_t old_state, vitaband_state_t new_state);
/**
 * @brief Processes raw sensor data and returns a risk score (1-10)
 */
uint8_t calculate_risk_score(float skin_temp, float base_skin_temp, uint8_t hr, uint8_t spo2);
/**
 * @brief Determines the state based on the 1-10 risk score.
 */
vitaband_state_t determine_state(uint8_t risk_score);

#endif