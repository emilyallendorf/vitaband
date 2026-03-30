#include "state_manager.h"
#include <zephyr/logging/log.h>

#include <stdlib.h>

bool emergency_long_press = false;


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

void state_manager_init(void) {
    LOG_INF("Initializing State Manager...");
    
    // Set the initial state
    // In mock mode, we just start at OK. 
    // In real mode, we might check sensors first.
    handle_state_transition(OK, OK); 
    
    LOG_INF("State Manager Ready.");
}

uint8_t calculate_risk_score(float skin_temp, float base_skin_temp, uint8_t heart_rate, uint8_t base_heart_rate) {
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
    if (tc_denominator <= 0.1f) tc_denominator = 0.1f;
    float hr_denominator = 180.0f-hr_0;
    if (hr_denominator <= 1.0f) hr_denominator = 01.0f;

    float psi = 5.0f * ((tc - tc_0) / tc_denominator)
                + 5.0f * (( hr - hr_0) / hr_denominator);
    psi = clampf (psi, 0.0f, 10.0f);
    // LOG_INF ("skin=%.2f base_skin=%.2f hr=%u base_hr=%u tc=%.2f psi=%.2f", (double)st, (double)st_0, heart_rate, base_heart_rate, (double)tc, (double)psi);
    return (uint8_t) (psi + 0.5f);
}

// vitaband_state_t determine_state(vitaband_state_t curr_state, float psi, bool emergency_button_pressed, bool emergency_button_long) {
vitaband_state_t determine_state(vitaband_state_t curr_state, float psi, button_status_t button_status) {
    static int64_t ge3_start_ms  = -1;   // PSI >= 3.0
    static int64_t ge7_start_ms  = -1;   // PSI >= 7.0
    static int64_t le65_start_ms = -1;   // PSI <= 6.5
    static int64_t le25_start_ms = -1;   // PSI <= 2.5

    int64_t now = k_uptime_get();
    vitaband_state_t next_state = curr_state;
    if (curr_state!=EMERGENCY && button_status==PRESSED){
        LOG_INF("State change: %d -> %d (emergency button pressed)", curr_state, EMERGENCY);
        ge3_start_ms  = -1;
        ge7_start_ms  = -1;
        le65_start_ms = -1;
        le25_start_ms = -1;
        return EMERGENCY;
    }
    if (curr_state==EMERGENCY)
    {
        if (button_status==LONG_PRESS)
        {
            LOG_INF("State change: %d -> %d (emergency button long-pressed)",
            EMERGENCY, WARNING);
            ge3_start_ms  = -1;
            ge7_start_ms  = -1;
            le65_start_ms = -1;
            le25_start_ms = -1;
            return WARNING;
        }
        return EMERGENCY;
    }
    if (psi >= 3.0f) {
        if (ge3_start_ms < 0) {
            ge3_start_ms = now;
        }
    } else {
        ge3_start_ms = -1;
    }

    if (psi >= 7.0f) {
        if (ge7_start_ms < 0) {
            ge7_start_ms = now;
        }
    } else {
        ge7_start_ms = -1;
    }

    if (psi <= 6.5f) {
        if (le65_start_ms < 0) {
            le65_start_ms = now;
        }
    } else {
        le65_start_ms = -1;
    }
    if (psi <= 2.5f) {
        if (le25_start_ms < 0) {
            le25_start_ms = now;
        }
    } else {
        le25_start_ms = -1;
    }
    switch (curr_state) {
    case OK:
        if (ge7_start_ms >= 0 && (now - ge7_start_ms) >= 1000) {
            next_state = CRITICAL;
        } else if (ge3_start_ms >= 0 && (now - ge3_start_ms) >= 3000) {
            next_state = WARNING;
        }
        break;

    case WARNING:
        if (ge7_start_ms >= 0 && (now - ge7_start_ms) >= 1000) {
            next_state = CRITICAL;
        } else if (le25_start_ms >= 0 && (now - le25_start_ms) >= 3000) {
            next_state = OK;
        }
        break;

    case CRITICAL:
        if (le65_start_ms >= 0 && (now - le65_start_ms) >= 1500) {
            next_state = WARNING;
        }
        break;
    default:
        next_state=OK;
        break;
        
    if (next_state != curr_state) {
        LOG_INF("State change: %d -> %d, psi=%.2f",
                curr_state, next_state, (double)psi);
        ge3_start_ms  = -1;
        ge7_start_ms  = -1;
        le65_start_ms = -1;
        le25_start_ms = -1;
        }
    
    }
    return next_state;
}

// vitaband_state_t determine_state(vitaband_state_t curr_state, float psi, bool button, bool long_press) {
//     if (button) return EMERGENCY;
    
//     if (psi >= 7.0f) return CRITICAL;
//     if (psi >= 3.0f) return WARNING;
    
//     return OK;
// }

/**
 * @brief Logic to execute when the system moves from one state to another.
 */
void handle_state_transition(vitaband_state_t old_state, vitaband_state_t new_state) {
    if (old_state == new_state) {
        return; // Safety check: do nothing if the state didn't actually change
    }

    LOG_INF("--- Transitioning: %d -> %d ---", old_state, new_state);

    /* 1. Perform one-shot actions based on the NEW state */
    switch (new_state) {
        case OK:
            LOG_INF("Action: System stabilized. Clearing alerts.");
            // haptics_stop_all(); // Example: Stop any warning vibrations
            break;

        case WARNING:
            LOG_WRN("Action: Vitals elevated. Triggering caution haptics.");
            break;

        case CRITICAL:
            LOG_ERR("Action: HIGH RISK. Preparing BLE Notification.");
            break;

        case EMERGENCY:
            LOG_ERR("Action: EMERGENCY! Activating Buzzer and BLE SOS.");
            break;

        default:
            break;
    }

    /* 2. Run the continuous state actions */
    execute_state_actions(new_state);
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