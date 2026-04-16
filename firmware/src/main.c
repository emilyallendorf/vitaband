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
#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_BT)
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#endif

#include <sensors.h>
#include <state_manager.h>
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
		printk("Connection failed, err 0x%02x %s\n", err, bt_hci_err_to_str(err));
	} else {
		printk("Connected\n");

		(void)atomic_set_bit(ble_state, STATE_CONNECTED);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected, reason 0x%02x %s\n", reason, bt_hci_err_to_str(reason));

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

	printk("Pairing cancelled: %s\n", addr);
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
			return LONG_PRESS;
		}
		return UNPRESSED;
	}

	if (button_press_time_ms > 0) {
		button_press_time_ms = 0;
		return PRESSED;
	}

	return UNPRESSED;
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

static float   base_skin_temp  = 34.0f;
static uint8_t base_heart_rate = 72;

int main(void)
{
	int err = 0;

	LOG_INF("VitaBand starting...");

#if IS_ENABLED(CONFIG_BT)
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	bt_conn_auth_cb_register(&auth_cb_display);

#else
	LOG_INF("Bluetooth disabled — same sensor loop and state machine as full build");
#endif

	temperature_sensor_init(BODY);
	temperature_sensor_init(AMBIENT);
	heart_rate_sensor_init();
	button_init();

	state_manager_init();

	k_msleep(500);
	float first_skin = read_temperature(BODY);
	if (first_skin > -50.0f) {
		base_skin_temp = first_skin;
		LOG_INF("Baseline skin temp: %.2f C", (double)base_skin_temp);
	} else {
		LOG_WRN("TMP117 not ready — using %.1f C fallback",
			(double)base_skin_temp);
	}

	calibrate_temperature_sensor(BODY);
	calibrate_temperature_sensor(AMBIENT);
	calibrate_heart_rate_sensor();
	bool sensors_ready = is_hr_sensor_ready() & is_temp_sensor_ready(BODY) &
			     is_temp_sensor_ready(AMBIENT);
	if (!sensors_ready) {
		LOG_WRN("Sensors not ready or need calibration (baseline skin %.1f C)",
			(double)base_skin_temp);
	}

	vitaband_state_t curr_state = OK;

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
	printk("Starting Legacy Advertising (connectable and scannable)\n");
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return 0;
	}

#else /* CONFIG_BT_EXT_ADV */
	printk("Creating a Coded PHY connectable non-scannable advertising set\n");
	err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
	if (err) {
		printk("Failed to create Coded PHY extended advertising set (err %d)\n", err);

		printk("Creating a non-Coded PHY connectable non-scannable advertising set\n");
		adv_param.options &= ~BT_LE_ADV_OPT_CODED;
		err = bt_le_ext_adv_create(&adv_param, NULL, &adv);
		if (err) {
			printk("Failed to create extended advertising set (err %d)\n", err);
			return 0;
		}
	}

	printk("Setting extended advertising data\n");
	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Failed to set extended advertising data (err %d)\n", err);
		return 0;
	}

	printk("Starting Extended Advertising (connectable non-scannable)\n");
	err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start extended advertising set (err %d)\n", err);
		return 0;
	}
#endif /* CONFIG_BT_EXT_ADV */

	printk("Advertising successfully started\n");

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
	printk("Vitaband Shell Test Harness Ready.\n");
#else
	printk("VitaBand: state machine + sensors (no BLE).\n");
#endif
	printk("Type 'help' in the terminal to see commands.\n");

	for (;;) {
		int64_t tick_start = k_uptime_get();

		float   skin_temp    = read_temperature(BODY);
		float   ambient_temp = read_temperature(AMBIENT);
		float   humidity     = read_humidity();
		uint8_t heart_rate   = read_heart_rate();

		uint8_t psi_int =
			calculate_risk_score(skin_temp, base_skin_temp, heart_rate, base_heart_rate);

		button_status_t btn = poll_button();

		vitaband_state_t next_state = determine_state(curr_state, (float)psi_int, btn);
		if (next_state != curr_state) {
			LOG_WRN("State transition: %s -> %s | psi=%u btn=%d",
				vitaband_state_name(curr_state), vitaband_state_name(next_state),
				psi_int, (int)btn);
			handle_state_transition(curr_state, next_state);
			curr_state = next_state;
		}

		LOG_INF("skin=%.2f amb=%.2f hum=%.0f%% hr=%u psi=%u state=%s btn=%d",
			(double)skin_temp, (double)ambient_temp, (double)humidity,
			heart_rate, psi_int, vitaband_state_name(curr_state), (int)btn);

#if IS_ENABLED(CONFIG_BT)
		if (vitaband_health_notify_enabled()) {
			(void)vitaband_health_notify(heart_rate, skin_temp, ambient_temp, curr_state);
		}

		if (atomic_test_and_clear_bit(ble_state, STATE_CONNECTED)) {
#if defined(HAS_LED)
			blink_stop();
#endif
		} else if (atomic_test_and_clear_bit(ble_state, STATE_DISCONNECTED)) {
#if !defined(CONFIG_BT_EXT_ADV)
			printk("Starting Legacy Advertising (connectable and scannable)\n");
			err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd,
					      ARRAY_SIZE(sd));
			if (err) {
				printk("Advertising failed to start (err %d)\n", err);
				return 0;
			}
#else
			printk("Starting Extended Advertising (connectable and non-scannable)\n");
			err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
			if (err) {
				printk("Failed to start extended advertising set (err %d)\n", err);
				return 0;
			}
#endif
#if defined(HAS_LED)
			blink_start();
#endif
		}
#endif /* CONFIG_BT */

		int64_t elapsed  = k_uptime_get() - tick_start;
		int32_t sleep_ms = (int32_t)(1000 - elapsed);
		if (sleep_ms > 0) {
			k_msleep(sleep_ms);
		}
	}

	return 0;
}
