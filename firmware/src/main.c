/* main.c - Application main entry point */

/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#if IS_ENABLED(CONFIG_BT)
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#endif

#include <sensors.h>
#include <state_manager.h>
#include <haptics.h>
#include <mock_sensors.h>
#include <config.h>
#if IS_ENABLED(CONFIG_BT)
#include <ble.h>
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#if IS_ENABLED(CONFIG_BT)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, VITABAND_HEALTH_SVC_UUID_VAL),
#if defined(CONFIG_BT_EXT_ADV)
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
#endif /* CONFIG_BT_EXT_ADV */
};

#if !defined(CONFIG_BT_EXT_ADV)
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};
#endif /* !CONFIG_BT_EXT_ADV */

enum {
	STATE_CONNECTED,
	STATE_DISCONNECTED,

	STATE_BITS,
};

static ATOMIC_DEFINE(ble_state, STATE_BITS);

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_WRN("Connection failed, err 0x%02x %s", err, bt_hci_err_to_str(err));
	} else {
		LOG_INF("Connected");

		(void)atomic_set_bit(ble_state, STATE_CONNECTED);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected, reason 0x%02x %s", reason, bt_hci_err_to_str(reason));

	(void)atomic_set_bit(ble_state, STATE_DISCONNECTED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_WRN("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

#endif /* CONFIG_BT */

#if defined(CONFIG_GPIO)
#include <zephyr/drivers/gpio.h>

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
#define HAS_LED     1
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#define BLINK_ONOFF K_MSEC(500)

static struct k_work_delayable blink_work;
static bool                  led_is_on;

static void blink_timeout(struct k_work *work)
{
	led_is_on = !led_is_on;
	gpio_pin_set(led.port, led.pin, (int)led_is_on);

	k_work_schedule(&blink_work, BLINK_ONOFF);
}

static int blink_setup(void)
{
	int err;

	printk("Checking LED device...");
	if (!gpio_is_ready_dt(&led)) {
		printk("failed.\n");
		return -EIO;
	}
	printk("done.\n");

	printk("Configuring GPIO pin...");
	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (err) {
		printk("failed.\n");
		return -EIO;
	}
	printk("done.\n");

	k_work_init_delayable(&blink_work, blink_timeout);

	return 0;
}

static void blink_start(void)
{
	printk("Start blinking LED...\n");
	led_is_on = false;
	gpio_pin_set(led.port, led.pin, (int)led_is_on);
	k_work_schedule(&blink_work, BLINK_ONOFF);
}

static void blink_stop(void)
{
	struct k_work_sync work_sync;

	printk("Stop blinking LED.\n");
	k_work_cancel_delayable_sync(&blink_work, &work_sync);

	/* Keep LED on */
	led_is_on = true;
	gpio_pin_set(led.port, led.pin, (int)led_is_on);
}
#endif /* LED0_NODE */

#if DT_HAS_ALIAS(emergency_button)
#define BUTTON_NODE DT_ALIAS(emergency_button)
#else
#define BUTTON_NODE DT_INVALID_NODE
#endif

#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)

#define LONG_PRESS_MS 1500

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static volatile int64_t button_press_time_ms = 0;
static volatile bool    button_active        = false;
/* State machine runs at 1 Hz; fast loop (~220 ms) used to drain HR FIFO. Raw PRESSED /
 * LONG_PRESS from poll would exist for only one fast iteration — latch until SM tick. */
static volatile bool button_latch_short = false;
static volatile bool button_latch_long  = false;
static struct gpio_callback button_cb_data;

static void button_isr(const struct device *dev, struct gpio_callback *cb,
		       uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	/* ACTIVE_LOW: logical 1 = pressed (pin physically LOW) */
	if (gpio_pin_get_dt(&button) == 1) {
		button_press_time_ms = k_uptime_get();
		button_active        = true;
	} else {
		button_active = false;
	}
}

static button_status_t poll_button(void)
{
	int64_t now = k_uptime_get();

	if (button_active) {
		if ((now - button_press_time_ms) >= LONG_PRESS_MS) {
			button_active = false;
			button_press_time_ms = 0;
			button_latch_short = false;
			button_latch_long  = true;
			// #region agent log
			LOG_DBG("btn: long press latched");
			// #endregion
			return LONG_PRESS;
		}
		return UNPRESSED;
	}

	if (button_press_time_ms > 0) {
		button_press_time_ms = 0;
		button_latch_short = true;
		// #region agent log
		LOG_DBG("btn: short press latched (release)");
		// #endregion
	}

	if (button_latch_long) {
		return LONG_PRESS;
	}
	if (button_latch_short) {
		return PRESSED;
	}

	return UNPRESSED;
}

static void button_clear_sm_latches(void)
{
	if (button_latch_short || button_latch_long) {
		// #region agent log
		LOG_DBG("btn: SM cleared latch short=%d long=%d", button_latch_short ? 1 : 0,
			button_latch_long ? 1 : 0);
		// #endregion
	}
	button_latch_short = false;
	button_latch_long  = false;
}

static int button_init(void)
{
	if (!gpio_is_ready_dt(&button)) {
		LOG_ERR("Button GPIO not ready");
		return -ENODEV;
	}
	int ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret) {
		LOG_ERR("Button configure failed: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_BOTH);
	if (ret) {
		LOG_ERR("Button interrupt configure failed: %d", ret);
		return ret;
	}

	gpio_init_callback(&button_cb_data, button_isr, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	LOG_INF("Button ready on P0.%d", button.pin);
	return 0;
}

#else /* GPIO on, but no emergency_button in DT or node disabled */

static int button_init(void)
{
	return 0;
}

static button_status_t poll_button(void)
{
	return UNPRESSED;
}

static void button_clear_sm_latches(void)
{
}

#endif /* DT_NODE_HAS_STATUS(BUTTON_NODE) */

#else /* !CONFIG_GPIO */

static int button_init(void)
{
	return 0;
}

static button_status_t poll_button(void)
{
	return UNPRESSED;
}

static void button_clear_sm_latches(void)
{
}

#endif /* CONFIG_GPIO */

static const char *vitaband_state_name(vitaband_state_t s)
{
	switch (s) {
	case OK:
		return "OK";
	case WARNING:
		return "WARNING";
	case CRITICAL:
		return "CRITICAL";
	case EMERGENCY:
		return "EMERGENCY";
	default:
		return "?";
	}
}

//static float   base_skin_temp  = 34.0f;
//static uint8_t base_heart_rate = 72;

static vitaband_state_t g_curr_state     = OK;
static bool             g_prev_mock_feed;

/* MAX86140 FIFO needs frequent draining (~220 ms); state machine + BLE stay at 1 Hz */
#define HR_POLL_MS          220
#define STATE_MACHINE_MS    1000

/* Same vitals behavior as main_ble_hr_body.c: fixed ambient in BLE payload, last good skin
 * when TMP117 read fails, EMA on HR.
 */
#define AMBIENT_C_FIXED 24.5f
#define BAD_TEMP_C      (-99.0f)

static uint8_t hr_smooth;
static float   last_body_c = 25.0f;
/** Last TMP117 read each poll (for logs); ~-99 °C means I2C/read failure — skin line still shows last_body_c. */
static float   last_body_raw = BAD_TEMP_C;

static uint8_t          g_latest_hr_bpm;
static button_status_t  g_latest_btn;

vitaband_state_t vitaband_current_state(void)
{
	return g_curr_state;
}

int main(void)
{
	int err = 0;

	LOG_INF("VitaBand starting...");

#if IS_ENABLED(CONFIG_BT)
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized");

	bt_conn_auth_cb_register(&auth_cb_display);

#else
	LOG_INF("Bluetooth disabled — same sensor loop and state machine as full build");
#endif

	/* Same order as main_ble_hr_body.c: recover I2C *before* tmp117_init (sensors.c). */
	k_msleep(250);

#define TMP117_I2C_NODE DT_PARENT(DT_NODELABEL(tmp117))
#if DT_NODE_HAS_STATUS(TMP117_I2C_NODE, okay)
	{
		const struct device *i2c = DEVICE_DT_GET(TMP117_I2C_NODE);

		LOG_INF("TMP117 bus ready: %s", device_is_ready(i2c) ? "yes" : "no");
		(void)i2c_recover_bus(i2c);
	}
#endif
#undef TMP117_I2C_NODE

	temperature_sensor_init(BODY);
	temperature_sensor_init(AMBIENT);
	heart_rate_sensor_init();
	button_init();

	err = haptics_init();
	if (err != 0) {
		LOG_WRN("haptics_init failed (%d) — buzzer/motor may not work", err);
	}

	LOG_INF("Sensor HW: MAX86140=%s TMP117=%s SHT3x=%s",
		sensors_hr_hw_initialized() ? "ok" : "FAIL",
		sensors_body_temp_hw_initialized() ? "ok" : "FAIL",
		sensors_ambient_temp_hw_initialized() ? "ok" : "FAIL");
	if (sensors_hr_hw_initialized()) {
		LOG_INF("HR may stay 0 until MAX86140 has PPG contact (finger).");
	}

	state_manager_init();

	k_msleep(500);
	float first_skin = read_temperature(BODY);
	if (first_skin > -50.0f) {
		base_skin_temp = first_skin;
		last_body_c    = first_skin;
		LOG_INF("Baseline skin temp: %.2f C", (double)base_skin_temp);
	} else {
		LOG_WRN("TMP117 not ready — using fallback baseline 34.0 C");
	}

	calibrate_temperature_sensor(BODY);
	calibrate_temperature_sensor(AMBIENT);
	calibrate_heart_rate_sensor();

	bool sensors_ready = is_hr_sensor_ready() && is_temp_sensor_ready(BODY) &&
			     is_temp_sensor_ready(AMBIENT);
	if (!sensors_ready) {
		LOG_WRN("Sensors not ready or need calibration (baseline skin %.1f C)",
			34.0);
	}

#if IS_ENABLED(CONFIG_BT)

#if defined(CONFIG_BT_EXT_ADV)
	struct bt_le_adv_param adv_param = {
		.id = BT_ID_DEFAULT,
		.sid = 0U,
		.secondary_max_skip = 0U,
		.options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_CODED),
		.interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
		.interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
		.peer = NULL,
	};
	struct bt_le_ext_adv *adv = NULL;
#endif

#if !defined(CONFIG_BT_EXT_ADV)
	LOG_INF("Starting Legacy Advertising (connectable and scannable)");
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return 0;
	}

#else /* CONFIG_BT_EXT_ADV */
	LOG_INF("Creating a Coded PHY connectable non-scannable advertising set");
	err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
	if (err) {
		LOG_WRN("Failed to create Coded PHY extended advertising set (err %d)", err);

		LOG_INF("Creating a non-Coded PHY connectable non-scannable advertising set");
		adv_param.options &= ~BT_LE_ADV_OPT_CODED;
		err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
		if (err) {
			LOG_ERR("Failed to create extended advertising set (err %d)", err);
			return 0;
		}
	}

	LOG_INF("Setting extended advertising data");
	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Failed to set extended advertising data (err %d)", err);
		return 0;
	}

	LOG_INF("Starting Extended Advertising (connectable non-scannable)");
	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		LOG_ERR("Failed to start extended advertising set (err %d)", err);
		return 0;
	}
#endif /* CONFIG_BT_EXT_ADV */

	LOG_INF("Advertising successfully started");

#if defined(HAS_LED)
	err = blink_setup();
	if (err) {
		return 0;
	}

	blink_start();
#endif /* HAS_LED */

#else /* !CONFIG_BT */

#if defined(HAS_LED)
	/* No advertising blink: state / haptics own led0 for transition testing */
	LOG_INF("LED blink disabled (no BLE); haptics patterns show state");
#endif

#endif /* CONFIG_BT */

#if IS_ENABLED(CONFIG_BT)
#if IS_ENABLED(CONFIG_SHELL)
	LOG_INF("VitaBand ready (BLE + shell). Type 'help' for commands.");
#else
	LOG_INF("VitaBand ready (BLE + RTT logs). Shell disabled in this build.");
#endif
#else
	LOG_INF("VitaBand: state machine + sensors (no BLE).");
#endif
#if IS_ENABLED(CONFIG_SHELL)
	LOG_INF("Interactive shell: type 'help' for commands.");
#endif

	int64_t next_state_tick_ms = k_uptime_get();

	for (;;) {
		int64_t tick_start = k_uptime_get();

		/* Advance scripted scenarios even without `test start` (was stuck otherwise). */
		mock_sensors_update_scenario();

		const bool mock_on = vitaband_test_harness_running() ||
				     mock_sensors_scenario_active();

		if (mock_on && !g_prev_mock_feed) {
			base_skin_temp  = mock_read_temperature();
			base_heart_rate = mock_read_heart_rate();
			LOG_INF("Mock feed on — PSI baseline skin=%.2f C base_hr=%u",
				(double)base_skin_temp, base_heart_rate);
		}
		g_prev_mock_feed = mock_on;

		if (mock_on) {
			g_latest_hr_bpm = mock_read_heart_rate();
			g_latest_btn = mock_read_button_status();
		} else {
			uint8_t raw_hr = read_heart_rate();

			if (raw_hr > 0U) {
				if (hr_smooth == 0U) {
					hr_smooth = raw_hr;
				} else {
					hr_smooth = (uint8_t)(((uint16_t)hr_smooth * 3U +
							       (uint16_t)raw_hr + 2U) /
							      4U);
				}
			} else {
				hr_smooth = 0U;
			}

			float body_c = read_temperature(BODY);

			last_body_raw = body_c;
			if (body_c > BAD_TEMP_C + 1.0f) {
				last_body_c = body_c;
			}

			g_latest_hr_bpm = hr_smooth;
			g_latest_btn = poll_button();
		}

#if IS_ENABLED(CONFIG_BT)
		if (atomic_test_and_clear_bit(ble_state, STATE_CONNECTED)) {
#if defined(HAS_LED)
			blink_stop();
#endif
		} else if (atomic_test_and_clear_bit(ble_state, STATE_DISCONNECTED)) {
#if !defined(CONFIG_BT_EXT_ADV)
			LOG_INF("Starting Legacy Advertising (connectable and scannable)");
			err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
			if (err) {
				LOG_ERR("Advertising failed to start (err %d)", err);
				return 0;
			}
#else
			LOG_INF("Starting Extended Advertising (connectable and non-scannable)");
			err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
			if (err) {
				LOG_ERR("Failed to start extended advertising set (err %d)", err);
				return 0;
			}
#endif
#if defined(HAS_LED)
			blink_start();
#endif
		}
#endif /* CONFIG_BT */

		int64_t now = k_uptime_get();
		if (now >= next_state_tick_ms) {
			next_state_tick_ms = now + STATE_MACHINE_MS;

			/*
			 * Snapshot vitals in one tick for PSI: same-cycle body temp + HR
			 * (not a stale g_latest_hr from an earlier 220 ms poll).
			 */
			float   skin_temp;
			uint8_t heart_rate;
			uint8_t hr_send;
			uint8_t hr_for_psi;

			if (mock_on) {
				skin_temp   = mock_read_temperature();
				heart_rate  = mock_read_heart_rate();
				hr_send     = heart_rate > 0U ? heart_rate : 1U;
				hr_for_psi  = heart_rate > 0U ? heart_rate : hr_send;
			} else {
				skin_temp  = last_body_c;
				heart_rate = hr_smooth;
				hr_send    = (hr_smooth > 0U) ? hr_smooth : 1U;
				hr_for_psi = (hr_smooth > 0U) ? hr_smooth : hr_send;
			}

			g_latest_hr_bpm = heart_rate;

			float ambient_temp = read_temperature(AMBIENT);
			float humidity     = read_humidity();

			uint8_t psi_int = calculate_risk_score(skin_temp, base_skin_temp, hr_for_psi,
							       base_heart_rate);

			vitaband_state_t next_state =
				determine_state(g_curr_state, (float)psi_int, g_latest_btn);
			if (next_state != g_curr_state) {
				LOG_WRN("State transition: %s -> %s | psi=%u btn=%d",
					vitaband_state_name(g_curr_state), vitaband_state_name(next_state),
					psi_int, (int)g_latest_btn);
				handle_state_transition(g_curr_state, next_state);
				g_curr_state = next_state;
			}

			button_clear_sm_latches();

			/* skin = last good body temp for PSI; body_raw = latest TMP117 read (-99 => failure). */
			LOG_INF("vitals: skin=%.2f C body_raw=%.2f C amb=%.2f C hum=%.0f%% HR=%u PSI=%u state=%s btn=%d mock=%d",
				(double)skin_temp,
				mock_on ? (double)skin_temp : (double)last_body_raw,
				(double)ambient_temp, (double)humidity,
				heart_rate, psi_int, vitaband_state_name(g_curr_state),
				(int)g_latest_btn, mock_on ? 1 : 0);

#if IS_ENABLED(CONFIG_BT)
			if (vitaband_health_notify_enabled()) {
				float body_for_notify = mock_on ? skin_temp : last_body_c;

				(void)vitaband_health_notify(hr_send, body_for_notify, AMBIENT_C_FIXED,
							     g_curr_state, psi_int);
			}
#endif
		}

		int64_t elapsed  = k_uptime_get() - tick_start;
		int32_t sleep_ms = (int32_t)(HR_POLL_MS - elapsed);
		if (sleep_ms > 0) {
			k_msleep(sleep_ms);
		}
	}

	return 0;
}
