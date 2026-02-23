#include "state_manager.h"
#include <zephyr/logging/log.h>

#include <stdlib.h>

LOG_MODULE_REGISTER(state_manager, LOG_LEVEL_INF);

uint8_t calculate_risk_score(float skin_temp, float base_skin_temp, 
uint8_t heart_rate, uint8_t base_heart_rate) {
    if (base_heart_rate == 0) return 0;
    if (base_skin_temp <= 0.0f) return 0;
    
    //psi calculation logic
    float st = skin_temp;
    float st_0 = base_skin_temp;
    float hr= (float) heart_rate;
    float hr_0 = (float) base_heart_rate;

    float st_denominator = 39.5f-st_0;
    if(st_denominator <= 0.1f) st_denominator = 0.1f;
    float hr_denominator = 180.0f-st_0;
    if(hr_denominator <= 1.0f) hr_denominator = 01.0f;

    

    

    
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
