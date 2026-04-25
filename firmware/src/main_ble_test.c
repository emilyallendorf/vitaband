#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>

#include <config.h>
#include <ble.h>

#ifndef CONFIG_BT_DEVICE_NAME
#define CONFIG_BT_DEVICE_NAME "VitaBand BLE Test"
#endif

#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static bool led_on;
#endif

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_DIS_VAL)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, VITABAND_HEALTH_SVC_UUID_VAL),
#if defined(CONFIG_BT_EXT_ADV)
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
#endif
};

#if !defined(CONFIG_BT_EXT_ADV)
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
		sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};
#define ADV_SD sd
#define ADV_SD_LEN ARRAY_SIZE(sd)
#else
#define ADV_SD NULL
#define ADV_SD_LEN 0
#endif

int main(void)
{
#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
	if (gpio_is_ready_dt(&led0)) {
		(void)gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	}
#endif

	/* #region agent log */
	printk("{\"sessionId\":\"75362d\",\"runId\":\"rtt-missing\",\"hypothesisId\":\"H_main_entry\",\"location\":\"main_ble_test:main\",\"message\":\"entered_main\",\"data\":{\"value\":1},\"timestamp\":%u}\n",
	       (unsigned int)k_uptime_get_32());
	/* #endregion */
	int err = bt_enable(NULL);
	/* #region agent log */
	printk("{\"sessionId\":\"75362d\",\"runId\":\"rtt-missing\",\"hypothesisId\":\"H_bt_enable\",\"location\":\"main_ble_test:main\",\"message\":\"bt_enable_return\",\"data\":{\"value\":%d},\"timestamp\":%u}\n",
	       err, (unsigned int)k_uptime_get_32());
	/* #endregion */

	if (err) {
		printk("ble-test: bt_enable failed (%d)\n", err);
		return 0;
	}

	printk("ble-test: bt enabled, starting advertising\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), ADV_SD, ADV_SD_LEN);
	/* #region agent log */
	printk("{\"sessionId\":\"75362d\",\"runId\":\"rtt-missing\",\"hypothesisId\":\"H_adv_start\",\"location\":\"main_ble_test:main\",\"message\":\"adv_start_return\",\"data\":{\"value\":%d},\"timestamp\":%u}\n",
	       err, (unsigned int)k_uptime_get_32());
	/* #endregion */
	if (err) {
		printk("ble-test: advertising failed (%d)\n", err);
		return 0;
	}

	printk("ble-test: advertising as \"%s\"\n", CONFIG_BT_DEVICE_NAME);

	uint8_t hr = 72U;
	float body_c = 33.8f;
	float amb_c = 24.5f;
	vitaband_state_t state = OK;

	while (1) {
#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
		if (gpio_is_ready_dt(&led0)) {
			led_on = !led_on;
			(void)gpio_pin_set_dt(&led0, led_on ? 1 : 0);
		}
#endif
		/* #region agent log */
		printk("{\"sessionId\":\"75362d\",\"runId\":\"rtt-missing\",\"hypothesisId\":\"H_loop_alive\",\"location\":\"main_ble_test:loop\",\"message\":\"loop_tick\",\"data\":{\"value\":%d},\"timestamp\":%u}\n",
		       (int)vitaband_health_notify_enabled(),
		       (unsigned int)k_uptime_get_32());
		/* #endregion */
		if (vitaband_health_notify_enabled()) {
			int nerr = vitaband_health_notify(hr, body_c, amb_c, state);
			/* #region agent log */
			printk("{\"sessionId\":\"75362d\",\"runId\":\"rtt-missing\",\"hypothesisId\":\"H_notify\",\"location\":\"main_ble_test:loop\",\"message\":\"notify_return\",\"data\":{\"value\":%d},\"timestamp\":%u}\n",
			       nerr, (unsigned int)k_uptime_get_32());
			/* #endregion */
			printk("ble-test: notify hr=%u body=%.2f amb=%.2f state=%u err=%d\n",
			       (unsigned int)hr, (double)body_c, (double)amb_c,
			       (unsigned int)state, nerr);
		} else {
			printk("ble-test: waiting for CCC subscribe\n");
		}

		hr = (uint8_t)((hr >= 110U) ? 72U : (hr + 2U));
		body_c = (body_c >= 35.2f) ? 33.8f : (body_c + 0.1f);
		amb_c = (amb_c >= 27.0f) ? 24.5f : (amb_c + 0.2f);
		state = (state == EMERGENCY) ? OK : (vitaband_state_t)(state + 1);

		k_sleep(K_SECONDS(1));
	}
}
