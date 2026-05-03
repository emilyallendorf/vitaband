/*
 * BLE + MAX86140 HR + TMP117 body temperature → VitaBand health telemetry notify.
 * Ambient °C is fixed (no SHT3x). Poll HR at the same cadence as main_max86140_hr.c.
 */

#include <errno.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <ble.h>
#include <config.h>
#include <max86140.h>
#include <state_manager.h>
#include <tmp117.h>

#define AMBIENT_C_FIXED    24.5f
#define BAD_TEMP_C         (-99.0f)
#define SAMPLE_PERIOD_MS   220U
#define NOTIFY_PERIOD_MS   1000U

#define LONG_PRESS_MS 1500U
#define BUZZ_PERIOD_NS 250000U
#define BUZZ_PULSE_NS  125000U

#if DT_HAS_ALIAS(emergency_button)
#define BUTTON_NODE DT_ALIAS(emergency_button)
#else
#define BUTTON_NODE DT_INVALID_NODE
#endif

#if DT_HAS_ALIAS(buzzer)
static const struct pwm_dt_spec s_buzzer = PWM_DT_SPEC_GET(DT_ALIAS(buzzer));
#endif

static const struct device *const s_gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static const uint8_t s_motor_en_pin = 4U;

#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
static const struct gpio_dt_spec s_button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static volatile int64_t s_button_press_ms;
static volatile bool s_button_active;
static struct gpio_callback s_button_cb;
#endif

#ifndef CONFIG_BT_DEVICE_NAME
#define CONFIG_BT_DEVICE_NAME "VitaBand BLE HR+Body"
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

#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	/* ACTIVE_LOW button in overlay -> logical 1 means pressed. */
	if (gpio_pin_get_dt(&s_button) == 1) {
		s_button_press_ms = k_uptime_get();
		s_button_active = true;
	} else {
		s_button_active = false;
	}
}

static bool button_pressed_event(void)
{
	int64_t now = k_uptime_get();

	if (s_button_active) {
		if ((now - s_button_press_ms) >= (int64_t)LONG_PRESS_MS) {
			s_button_active = false;
		}
		return false;
	}

	if (s_button_press_ms > 0) {
		s_button_press_ms = 0;
		return true;
	}

	return false;
}
#endif

static int buzzer_set(bool on)
{
#if DT_HAS_ALIAS(buzzer)
	uint32_t pulse = on ? BUZZ_PULSE_NS : 0U;
	return pwm_set_dt(&s_buzzer, BUZZ_PERIOD_NS, pulse);
#else
	ARG_UNUSED(on);
	return 0;
#endif
}

int main(void)
{
	int err;
	bool buzzer_on = true;

	printk("ble-hr-body: boot\n");

	k_sleep(K_MSEC(250));

#define TMP117_I2C_NODE DT_PARENT(DT_NODELABEL(tmp117))

#if DT_NODE_HAS_STATUS(TMP117_I2C_NODE, okay)
	{
		const struct device *i2c = DEVICE_DT_GET(TMP117_I2C_NODE);

		printk("ble-hr-body: TMP117 bus ready: %s\n",
		       device_is_ready(i2c) ? "yes" : "no");
		(void)i2c_recover_bus(i2c);
	}
#endif

	err = tmp117_init();
	if (err != 0) {
		printk("ble-hr-body: TMP117 init failed (%d)", err);
		if (err == -EIO) {
			printk(" (-EIO: no I2C ACK from TMP117)\n");
		} else {
			printk("\n");
		}
	}

	err = max86140_init();
	if (err != 0) {
		printk("ble-hr-body: max86140_init failed (%d)\n", err);
	}

#if DT_HAS_ALIAS(buzzer)
	if (!pwm_is_ready_dt(&s_buzzer)) {
		printk("ble-hr-body: buzzer PWM not ready\n");
	} else {
		err = buzzer_set(true);
		printk("ble-hr-body: buzzer init ON (err=%d)\n", err);
	}
#else
	printk("ble-hr-body: no DT alias 'buzzer' (buzzer control disabled)\n");
#endif

	if (device_is_ready(s_gpio0)) {
		(void)gpio_pin_configure(s_gpio0, s_motor_en_pin, GPIO_OUTPUT_HIGH);
	}

#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
	if (!gpio_is_ready_dt(&s_button)) {
		printk("ble-hr-body: emergency button GPIO not ready\n");
	} else {
		err = gpio_pin_configure_dt(&s_button, GPIO_INPUT);
		if (err == 0) {
			err = gpio_pin_interrupt_configure_dt(&s_button, GPIO_INT_EDGE_BOTH);
		}
		if (err == 0) {
			gpio_init_callback(&s_button_cb, button_isr, BIT(s_button.pin));
			gpio_add_callback(s_button.port, &s_button_cb);
			printk("ble-hr-body: button on P0.%u toggles buzzer\n",
			       (unsigned int)s_button.pin);
		} else {
			printk("ble-hr-body: button init failed (%d)\n", err);
		}
	}
#else
	printk("ble-hr-body: no DT alias 'emergency-button' (toggle disabled)\n");
#endif

	err = bt_enable(NULL);
	if (err != 0) {
		printk("ble-hr-body: bt_enable failed (%d)\n", err);
		return 0;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), ADV_SD, ADV_SD_LEN);
	if (err != 0) {
		printk("ble-hr-body: advertising failed (%d)\n", err);
		return 0;
	}

	printk("ble-hr-body: advertising as \"%s\"\n", CONFIG_BT_DEVICE_NAME);

	static float last_body_c = 25.0f;
	static uint8_t hr_smooth;
	uint32_t last_notify = 0U;
	/* PSI baselines — align with main.c defaults; adjust after stable wear if desired */
	static float base_skin_c = 34.0f;
	static uint8_t base_hr_bpm = 72U;

	for (;;) {
#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
		if (button_pressed_event()) {
			buzzer_on = !buzzer_on;
			err = buzzer_set(buzzer_on);
			printk("ble-hr-body: button press -> buzzer %s (err=%d)\n",
			       buzzer_on ? "ON" : "OFF", err);
		}
#endif

		uint8_t raw = max86140_read_heartrate();

		if (raw > 0U) {
			if (hr_smooth == 0U) {
				hr_smooth = raw;
			} else {
				hr_smooth = (uint8_t)(((uint16_t)hr_smooth * 3U + (uint16_t)raw + 2U) / 4U);
			}
		} else {
			hr_smooth = 0U;
		}

	// PSI baselines — use body temp and HR at stable wear time, or defaults if never calibrated.

		float body_c = tmp117_read_temperature();

		if (body_c <= BAD_TEMP_C + 1.0f) {
			/* keep last_body_c */
		} else {
			last_body_c = body_c;
		}

		uint32_t now = k_uptime_get_32();

		if (now - last_notify >= NOTIFY_PERIOD_MS) {
			last_notify = now;

			if (vitaband_health_notify_enabled()) {
				uint8_t hr_send = (hr_smooth > 0U) ? hr_smooth : 1U;
				uint8_t hr_for_psi =
					(hr_smooth > 0U) ? hr_smooth : hr_send;
				uint8_t risk = calculate_risk_score(last_body_c, base_skin_c, hr_for_psi,
								    base_hr_bpm);
				printk("ble-hr-body: HR=%u body=%.2f C risk=%u\n",
				       (unsigned int)hr_send, (double)last_body_c, (unsigned int)risk);

				err = vitaband_health_notify(hr_send, last_body_c, AMBIENT_C_FIXED, OK,
							   risk);
				if (err != 0) {
					printk("ble-hr-body: notify err %d\n", err);
				}
			} else {
				printk("ble-hr-body: HR=%u body=%.2f C (no BLE notify)\n",
				       (unsigned int)hr_smooth, (double)last_body_c);
			}
		}

		k_sleep(K_MSEC(SAMPLE_PERIOD_MS));
	}
}
