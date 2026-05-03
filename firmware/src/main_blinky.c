/*
 * Minimal bring-up:
 *   MOTOR_EN P0.04 — GPIO high (enable).
 *   BUZZER_EN P0.05 — hardware PWM ~4 kHz when “on” (piezo).
 *   Emergency button (P0.17, ACTIVE_LOW): short press toggles buzzer PWM on/off.
 *
 * printk: Segger RTT on the DK (not the virtual COM port).
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define HAPTIC_PORT DEVICE_DT_GET(DT_NODELABEL(gpio0))
#define PWM_DEV     DEVICE_DT_GET(DT_NODELABEL(pwm0))

#define MOTOR_PIN 4U
#define PWM_CH    0U

#define BUZZ_PERIOD_NS 250000U
#define BUZZ_PULSE_NS  125000U

#define LONG_PRESS_MS 1500

enum { BTN_UNPRESSED, BTN_PRESSED, BTN_LONG_PRESS };

#if DT_HAS_ALIAS(emergency_button)
#define BUTTON_NODE DT_ALIAS(emergency_button)
#else
#define BUTTON_NODE DT_INVALID_NODE
#endif

#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)

static const struct gpio_dt_spec s_button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static volatile int64_t s_button_press_ms;
static volatile bool    s_button_active;
static struct gpio_callback s_button_cb;

static void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);
	if (gpio_pin_get_dt(&s_button) == 1) {
		s_button_press_ms = k_uptime_get();
		s_button_active = true;
	} else {
		s_button_active = false;
	}
}

static int poll_button(void)
{
	int64_t now = k_uptime_get();

	if (s_button_active) {
		if ((now - s_button_press_ms) >= LONG_PRESS_MS) {
			s_button_active = false;
			return BTN_LONG_PRESS;
		}
		return BTN_UNPRESSED;
	}

	if (s_button_press_ms > 0) {
		s_button_press_ms = 0;
		return BTN_PRESSED;
	}

	return BTN_UNPRESSED;
}

static int button_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&s_button)) {
		printk("ERROR: button GPIO not ready\n");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&s_button, GPIO_INPUT);
	if (ret != 0) {
		printk("ERROR: button configure %d\n", ret);
		return ret;
	}
	ret = gpio_pin_interrupt_configure_dt(&s_button, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printk("ERROR: button irq %d\n", ret);
		return ret;
	}
	gpio_init_callback(&s_button_cb, button_isr, BIT(s_button.pin));
	gpio_add_callback(s_button.port, &s_button_cb);
	printk("OK: emergency button on P0.%02u\n", s_button.pin);
	return 0;
}

#else

static int button_init(void)
{
	return 0;
}

static int poll_button(void)
{
	return BTN_UNPRESSED;
}

#endif

int main(void)
{
	int ret;
#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
	bool buzzer_on = true;
#endif

	printk("\n=== vitaband bring-up (button toggles buzzer PWM) ===\n");

	if (!device_is_ready(HAPTIC_PORT)) {
		printk("ERROR: gpio0 not ready\n");
		return 0;
	}
	printk("OK: gpio0 ready\n");

	if (!device_is_ready(PWM_DEV)) {
		printk("ERROR: pwm0 not ready\n");
		return 0;
	}
	printk("OK: pwm0 ready\n");

	ret = button_init();
	if (ret != 0) {
		printk("WARN: no button — no buzzer toggle\n");
	}

	ret = gpio_pin_configure(HAPTIC_PORT, MOTOR_PIN, GPIO_OUTPUT_HIGH);
	if (ret != 0) {
		printk("ERROR: motor GPIO %d\n", ret);
		return 0;
	}
	printk("OK: P0.04 MOTOR_EN high\n");

	ret = pwm_set(PWM_DEV, PWM_CH, BUZZ_PERIOD_NS, BUZZ_PULSE_NS, PWM_POLARITY_NORMAL);
	if (ret != 0) {
		printk("ERROR: pwm idle failed %d\n", ret);
		return 0;
	}

	for (;;) {
#if DT_NODE_HAS_STATUS(BUTTON_NODE, okay)
		int btn = poll_button();
		if (btn == BTN_PRESSED) {
			buzzer_on = !buzzer_on;
			if (buzzer_on) {
				ret = pwm_set(PWM_DEV, PWM_CH, BUZZ_PERIOD_NS, BUZZ_PULSE_NS,
					      PWM_POLARITY_NORMAL);
				printk("Buzzer ON (~4 kHz), err=%d\n", ret);

				
			} else {
				ret = pwm_set(PWM_DEV, PWM_CH, BUZZ_PERIOD_NS, 0U,
					      PWM_POLARITY_NORMAL);
				printk("Buzzer OFF, err=%d\n", ret);

				ret = gpio_pin_configure(HAPTIC_PORT, MOTOR_PIN, GPIO_OUTPUT_LOW);
				if (ret != 0) {
					printk("ERROR: motor GPIO %d\n", ret);
					return 0;
				}
				printk("OK: P0.04 MOTOR_EN low\n");
			}
		} else if (btn == BTN_LONG_PRESS) {
			printk("Long press (ignored in bring-up)\n");
		}
#endif
		k_sleep(K_MSEC(20));
	}
}
