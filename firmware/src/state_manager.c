#include "state_manager.h"
#include <zephyr/logging/log.h>

#include <stdlib.h>

LOG_MODULE_REGISTER(state_manager, LOG_LEVEL_INF);

uint8_t calculate_risk_score(float skin_temp, uint8_t heart_rate) {
    uint8_t score = rand(10);
    return score;
}

vitaband_state_t determine_state(uint8_t risk_score) {
    // Mapping the 1-10 scale to our 4 levels
    switch (risk_score) {
        case 1 ... 3:
            return OK;
        case 4 ... 6:
            return WARNING;
        case 7 ... 8:
            return CRITICAL;
        case 9 ... 10:
            return EMERGENCY;
        default:
            return OK;
    }
}

void execute_state_actions(vitaband_state_t state) {
    switch (state) {
        case OK:
            // Pulse Green LED slowly
            // LOG_INF("All clear: System Green");
            break;

        case WARNING:
            // Solid Yellow LED
            // LOG_WRN("Caution: Vitals elevated");
            break;

        case CRITICAL:
            // send BLE notification
            // LOG_ERR("Warning: High Risk detected!");
            break;

        case EMERGENCY:
            // Sound Buzzer, high-priority BLE Alert
            // LOG_ERR("CRITICAL: EMERGENCY THRESHOLD!");
            break;

        default:
            break;
    }
}