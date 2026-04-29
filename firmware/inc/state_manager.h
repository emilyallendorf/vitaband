#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <stdint.h>
#include <config.h>
#include <stdbool.h>

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

typedef enum {
    UNPRESSED,
    PRESSED,
    LONG_PRESS
} button_status_t;

void state_manager_init(void);

// const char* get_state_string(enum device_state state);


void handle_state_transition(vitaband_state_t old_state, vitaband_state_t new_state);

/**
 * @brief Processes raw sensor data and returns a risk score (1-10)
 */
uint8_t calculate_risk_score(float skin_temp, float base_skin_temp, uint8_t hr,
			     uint8_t base_heart_rate);

/**
 * @brief Determines the state based on the 1-10 risk score.
 */
vitaband_state_t determine_state(vitaband_state_t curr_state, float psi, button_status_t status);

/** Current vitals state driven by `main.c` (single source of truth for shell stats). */
vitaband_state_t vitaband_current_state(void);

#endif