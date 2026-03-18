#include "state_manager.h"
#include <zephyr/logging/log.h>

#include <stdlib.h>



LOG_MODULE_REGISTER(state_manager, LOG_LEVEL_INF);
#define BASE_CORE_TEMP_C   37.0f
#define B_T                0.25f
#define B_H                0.01f

static float clampf (float x, float lo, float hi)
{
    if (x<lo) return lo;
    if (x>hi) return hi;
    return x;
}

uint8_t calculate_risk_score(float skin_temp, float base_skin_temp, 
uint8_t heart_rate, uint8_t base_heart_rate) {
    if (base_heart_rate == 0) return 0;
    if (base_skin_temp <= 0.0f) return 0;
    
    //psi calculation logic
    float st = skin_temp;
    float st_0 = base_skin_temp;
    float hr= (float) heart_rate;
    float hr_0 = (float) base_heart_rate;

    //core temperature calculation
    float tc_0 = BASE_CORE_TEMP_C;
    float tc = tc_0 + B_T * (st-st_0) + B_H * (hr-hr_0);
    
    //psi calculation
    float tc_denominator = 39.5f-tc_0;
    if(tc_denominator <= 0.1f) tc_denominator = 0.1f;
    float hr_denominator = 180.0f-hr_0;
    if(hr_denominator <= 1.0f) hr_denominator = 01.0f;

    float psi = 5.0f * ((tc - tc_0) / tc_denominator)
                + 5.0f * (( hr - hr_0) / hr_denominator);
    psi = clampf (psi, 0.0f, 10.0f);
    LOG_INF ("skin=%.2f base_skin=%.2f hr=%u base_hr=%u tc=%.2f psi=%.2f",
            (double)st, (double)st_0,
            heart_rate, base_heart_rate,
            (double)tc, (double)psi);
    return (uint8_t) (psi + 0.5f);
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
