/*
 * Firmware Test Harness
 * Tests state machine, power management, and haptics with mock sensors
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <stdlib.h>
#include "mock_sensors.h"
#include "state_manager.h"
#include "power.h"
#include "haptics.h"

LOG_MODULE_REGISTER(test_harness, LOG_LEVEL_INF);

/* ========================================================================== */
/* TEST STATE                                                                 */
/* ========================================================================== */

static bool test_running = false;
static vitaband_state_t current_state = STATE_OK;
static vitaband_state_t previous_state = STATE_OK;

/* Statistics */
static struct {
    uint32_t state_ok_count;
    uint32_t state_warning_count;
    uint32_t state_emergency_count;
    uint32_t total_transitions;
    uint32_t last_transition_time;
} test_stats = {0};

const char* get_state_string(vitaband_state_t state) {
     switch (state) {
        case 0:
            return "OK";
        case 1:
            return "WARNING";
        case 2:
            return "CRITICAL";
        case 3:
            return "EMERGENCY";
        default:
            return "OK";
    }
}

const char* get_status_string(button_status_t status) {
     switch (status) {
        case 0:
            return "UNPRESSED";
        case 1:
            return "PRESSED";
        case 2:
            return "LONG_PRESS";
        default:
            return "UNDEFINED";
    }
}


/* ========================================================================== */
/* TEST LOOP                                                                  */
/* ========================================================================== */

static void test_loop_thread(void *arg1, void *arg2, void *arg3)
{
    printk("!!! THREAD IS ALIVE: Entering Loop !!!\n");
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    
    LOG_INF("=== Test Harness Started ===");
    float base_temp = mock_read_temperature();
    uint8_t base_hr = mock_read_heart_rate();
    
    while (test_running) {
        mock_sensors_update_scenario();

        /* 1. Get the numbers */
        uint8_t hr = mock_read_heart_rate();
        float temp = mock_read_temperature();
        button_status_t status = mock_read_button_status();
        
        /* 2. Calculate PSI */
        uint8_t score = calculate_risk_score(temp, base_temp, hr, base_hr);
        float psi_value = (float)score; // Or however your math returns PSI

        /* 3. FORCE THE UPDATE */
        // Pass the  previous state so the state machine knows where it is
        current_state = determine_state(previous_state, psi_value, status);

        /* 4. DEBUG EVERYTHING */
        printk("DEBUG: PSI=%.1f, CurrState=%s\n", 
(double)psi_value, get_state_string(current_state));

        /* 5. The Transition Check */
        if (current_state != previous_state) {
            printk("!!! TRANSITION DETECTED !!!\n");
            
            LOG_WRN("STATE CHANGE: %s -> %s", 
                    get_state_string(previous_state), 
                    get_state_string(current_state));

            handle_state_transition(previous_state, current_state);
            
            /* CRITICAL: Update the previous state so we don't trigger again next second */
            previous_state = current_state;
        }

        k_msleep(1000);
    }
}

/* ========================================================================== */
/* TEST CONTROL                                                               */
/* ========================================================================== */
K_THREAD_STACK_DEFINE(test_stack, 2048);
    static struct k_thread test_thread_data;
    static k_tid_t test_thread;

void test_harness_start(void)
{
    if (test_running) {
        LOG_WRN("Test already running");
        return;
    }

    
    LOG_INF("Starting test harness");
    
    /* Initialize mock sensors */
    mock_sensors_init();
    
    // /* Initialize state manager */
    state_manager_init();
    
    /* Initialize haptics */
    haptics_init();
    
    /* Start test thread */

    test_running = true;
    
    test_thread = k_thread_create(&test_thread_data,
                             test_stack,
                             K_THREAD_STACK_SIZEOF(test_stack),
                             test_loop_thread,
                             NULL, NULL, NULL,
                             1, 0, K_NO_WAIT);
    printk("DEBUG: k_thread_start() called successfully.\n");
}

void test_harness_stop(void)
{
    if (!test_running) {
        LOG_WRN("Test not running");
        return;
    }
    
    LOG_INF("Stopping test harness");
    test_running = false;
    
    /* Stop all haptics */
    haptics_stop_all();
}

bool vitaband_test_harness_running(void)
{
	return test_running;
}

void test_harness_print_stats(void)
{
    uint32_t total_samples = test_stats.state_ok_count +
                             test_stats.state_warning_count +
                             test_stats.state_emergency_count;
    
    LOG_INF("=== Test Statistics ===");
    LOG_INF("Total samples: %u", total_samples);
    LOG_INF("State OK: %u (%.1f%%)",
            test_stats.state_ok_count,
            total_samples > 0 ? (test_stats.state_ok_count * 100.0f / total_samples) : 0);
    LOG_INF("State WARNING: %u (%.1f%%)",
            test_stats.state_warning_count,
            total_samples > 0 ? (test_stats.state_warning_count * 100.0f / total_samples) : 0);
    LOG_INF("State EMERGENCY: %u (%.1f%%)",
            test_stats.state_emergency_count,
            total_samples > 0 ? (test_stats.state_emergency_count * 100.0f / total_samples) : 0);
    LOG_INF("Total transitions: %u", test_stats.total_transitions);
    
    if (test_stats.last_transition_time > 0) {
        uint32_t time_since_transition = (k_uptime_get_32() - test_stats.last_transition_time) / 1000;
        LOG_INF("Last transition: %u seconds ago", time_since_transition);
    }
    
    LOG_INF("Current state: %s", get_state_string(current_state));
}

void test_harness_reset_stats(void)
{
    memset(&test_stats, 0, sizeof(test_stats));
    LOG_INF("Statistics reset");
}

/* ========================================================================== */
/* SHELL COMMANDS                                                             */
/* ========================================================================== */

static int cmd_test_start(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    test_harness_start();
    shell_print(sh, "Test harness started");
    return 0;
}

static int cmd_test_stop(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    test_harness_stop();
    shell_print(sh, "Test harness stopped");
    return 0;
}

static int cmd_test_stats(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    test_harness_print_stats();
    return 0;
}

static int cmd_test_reset(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    test_harness_reset_stats();
    shell_print(sh, "Statistics reset");
    return 0;
}

/* Mock sensor commands */
static int cmd_mock_hr(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: mock hr <bpm>");
        return -EINVAL;
    }
    
    int hr = atoi(argv[1]);
    if (hr < 0 || hr > 255) {
        shell_error(sh, "HR must be 0-255");
        return -EINVAL;
    }
    
    mock_sensors_set_heart_rate(hr);
    shell_print(sh, "Heart rate set to %d BPM", hr);
    return 0;
}

static int cmd_mock_temp(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: mock temp <celsius>");
        return -EINVAL;
    }
    
    float temp = atof(argv[1]);
    if (temp < 30.0f || temp > 45.0f) {
        shell_error(sh, "Temperature must be 30.0-45.0");
        return -EINVAL;
    }
    
    mock_sensors_set_temperature(temp);
    shell_print(sh, "Temperature set to %.1f°C", temp);
    return 0;
}

static int cmd_mock_button(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: mock button <status>");
        return -EINVAL;
    }
    
    button_status_t status;

    if (strcmp(argv[1], "PRESSED") == 0)  status = PRESSED;
    else if (strcmp(argv[1], "UNPRESSED") == 0) status = UNPRESSED;
    else if (strcmp(argv[1], "LONG_PRESS") == 0) status = LONG_PRESS;
    else {
        shell_error(sh, "Invalid status. Use PRESSED or UNPRESSED or LONG_PRESS");
        return -EINVAL;
    }
    
    mock_sensors_set_button_status(status);
    shell_print(sh, "Button status set to %s", get_status_string(status));
    return 0;
}

static int cmd_mock_battery(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: mock battery <mv>");
        return -EINVAL;
    }
    
    int voltage = atoi(argv[1]);
    if (voltage < 2500 || voltage > 4500) {
        shell_error(sh, "Battery must be 2500-4500 mV");
        return -EINVAL;
    }
    
    mock_sensors_set_battery(voltage);
    shell_print(sh, "Battery set to %d mV", voltage);
    return 0;
}

static int cmd_mock_mode(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: mock mode <static|random|sine>");
        return -EINVAL;
    }
    
    mock_mode_t mode;
    if (strcmp(argv[1], "static") == 0) {
        mode = MOCK_MODE_STATIC;
    } else if (strcmp(argv[1], "random") == 0) {
        mode = MOCK_MODE_RANDOM;
    } else if (strcmp(argv[1], "sine") == 0) {
        mode = MOCK_MODE_SINE_WAVE;
    } else {
        shell_error(sh, "Unknown mode. Use: static, random, or sine");
        return -EINVAL;
    }
    
    mock_sensors_set_mode(mode);
    shell_print(sh, "Mock mode set to %s", argv[1]);
    return 0;
}

static int cmd_mock_noise(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 3) {
        shell_error(sh, "Usage: mock noise <on|off> <amplitude>");
        return -EINVAL;
    }
    
    bool enable = (strcmp(argv[1], "on") == 0);
    int amplitude = atoi(argv[2]);
    
    if (amplitude < 0 || amplitude > 50) {
        shell_error(sh, "Amplitude must be 0-50");
        return -EINVAL;
    }
    
    mock_sensors_enable_noise(enable, amplitude);
    shell_print(sh, "Noise %s (amplitude: %d)", enable ? "enabled" : "disabled", amplitude);
    return 0;
}

static int cmd_scenario(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: scenario <name|stop|list>");
        return -EINVAL;
    }
    
    if (strcmp(argv[1], "stop") == 0) {
        mock_sensors_stop_scenario();
        shell_print(sh, "Scenario stopped");
        return 0;
    }
    
    if (strcmp(argv[1], "list") == 0) {
        shell_print(sh, "Available scenarios:");
        shell_print(sh, "  normal      - Normal day (all OK)");
        shell_print(sh, "  exercise    - Exercise session");
        shell_print(sh, "  fever       - Fever development");
        shell_print(sh, "  tachycardia - Heart rate spike");
        shell_print(sh, "  battery     - Battery drain");
        shell_print(sh, "  emergency   - Multi-parameter emergency");
        return 0;
    }
    
    /* Map name to scenario */
    mock_scenario_t scenario;
    if (strcmp(argv[1], "normal") == 0) {
        scenario = SCENARIO_NORMAL;
    } else if (strcmp(argv[1], "exercise") == 0) {
        scenario = SCENARIO_EXERCISE;
    } else if (strcmp(argv[1], "fever") == 0) {
        scenario = SCENARIO_FEVER;
    } else if (strcmp(argv[1], "tachycardia") == 0) {
        scenario = SCENARIO_TACHYCARDIA;
    } else if (strcmp(argv[1], "battery") == 0) {
        scenario = SCENARIO_BATTERY_DRAIN;
    } else if (strcmp(argv[1], "emergency") == 0) {
        scenario = SCENARIO_EMERGENCY;
    } else {
        shell_error(sh, "Unknown scenario. Use 'scenario list' to see options");
        return -EINVAL;
    }
    
    mock_sensors_start_scenario(scenario);
    shell_print(sh, "Started scenario: %s", mock_sensors_get_scenario_name(scenario));
    return 0;
}

/* Haptics test commands */
static int cmd_haptics_test(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    
    shell_print(sh, "Running haptics test sequence...");
    haptics_test_all();
    return 0;
}

static int cmd_haptics_led(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: haptics led <on|off>");
        return -EINVAL;
    }
    
    bool on = (strcmp(argv[1], "on") == 0);
    haptics_led_set(on);
    shell_print(sh, "LED %s", on ? "ON" : "OFF");
    return 0;
}

/* Register shell commands */
SHELL_STATIC_SUBCMD_SET_CREATE(test_cmds,
    SHELL_CMD(start, NULL, "Start test harness", cmd_test_start),
    SHELL_CMD(stop, NULL, "Stop test harness", cmd_test_stop),
    SHELL_CMD(stats, NULL, "Show statistics", cmd_test_stats),
    SHELL_CMD(reset, NULL, "Reset statistics", cmd_test_reset),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(mock_cmds,
    SHELL_CMD(hr, NULL, "Set heart rate <bpm>", cmd_mock_hr),
    SHELL_CMD(temp, NULL, "Set temperature <celsius>", cmd_mock_temp),
    SHELL_CMD(battery, NULL, "Set battery <mv>", cmd_mock_battery),
    SHELL_CMD(mode, NULL, "Set mode <static|random|sine>", cmd_mock_mode),
    SHELL_CMD(noise, NULL, "Set noise <on|off> <amplitude>", cmd_mock_noise),
    SHELL_CMD(button, NULL, "Set button <status>", cmd_mock_button),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(haptics_cmds,
    SHELL_CMD(test, NULL, "Test all haptics", cmd_haptics_test),
    SHELL_CMD(led, NULL, "Control LED <on|off>", cmd_haptics_led),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(test, &test_cmds, "Test harness commands", NULL);
SHELL_CMD_REGISTER(mock, &mock_cmds, "Mock sensor commands", NULL);
SHELL_CMD_REGISTER(scenario, NULL, "Scenario commands", cmd_scenario);
SHELL_CMD_REGISTER(haptics, &haptics_cmds, "Haptics test commands", NULL);