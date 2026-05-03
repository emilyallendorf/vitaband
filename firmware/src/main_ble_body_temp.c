/*
 * BLE + TMP117 body temperature: reads skin/body °C from the board TMP117 node,
 * sends telemetry notify (same GATT as production). HR and ambient temp are fixed.
 *
 * I2C bring-up matches main_tmp117_i2c.c: short delay + bus recover before probe
 * (cold start / reset can otherwise see -EIO / no ACK on first access).
 */

#include <errno.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>

#include <config.h>
#include <ble.h>
#include <state_manager.h>
#include <tmp117.h>

#define HR_BPM_FIXED       72U
#define AMBIENT_C_FIXED    24.5f
#define BAD_TEMP_C         (-99.0f)

#define TMP117_I2C_ADDR DT_REG_ADDR(DT_NODELABEL(tmp117))

#ifndef CONFIG_BT_DEVICE_NAME
#define CONFIG_BT_DEVICE_NAME "VitaBand BLE Body"
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
	int err;

	k_sleep(K_MSEC(250));

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
	{
		const struct device *i2c = DEVICE_DT_GET(DT_NODELABEL(i2c0));

		printk("ble-body: i2c0 ready: %s\n",
		       device_is_ready(i2c) ? "yes" : "no (overlay / TWIM binding)");
		(void)i2c_recover_bus(i2c);
	}
#else
	printk("ble-body: i2c0 not okay in devicetree — check overlay enables TWIM + tmp117\n");
#endif

	err = tmp117_init();

	if (err != 0) {
		printk("ble-body: TMP117 init failed (%d)", err);
		if (err == -EIO) {
			printk(" (-EIO: no ACK at DT addr 0x%02x)\n", (unsigned int)TMP117_I2C_ADDR);
			printk("ble-body: DK Arduino I2C pin order? Try:\n");
			printk("ble-body:   -DDTC_OVERLAY_FILE=\"app_tmp117_i2c.overlay;app_tmp117_i2c_dkswap.overlay\"\n");
		} else {
			printk("\n");
		}
	}

	err = bt_enable(NULL);
	if (err != 0) {
		printk("ble-body: bt_enable failed (%d)\n", err);
		return 0;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), ADV_SD, ADV_SD_LEN);
	if (err != 0) {
		printk("ble-body: advertising failed (%d)\n", err);
		return 0;
	}

	printk("ble-body: advertising as \"%s\" (HR=%u ambient=%.1f const)\n",
	       CONFIG_BT_DEVICE_NAME, (unsigned int)HR_BPM_FIXED,
	       (double)AMBIENT_C_FIXED);

	static float last_body_c = 25.0f;
	static float base_skin_c = 34.0f;

	while (1) {
		float body_c = tmp117_read_temperature();

		if (body_c <= BAD_TEMP_C + 1.0f) {
			printk("ble-body: TMP117 read error / not ready (using last %.2f C)\n",
			       (double)last_body_c);
		} else {
			last_body_c = body_c;
			printk("ble-body: body=%.2f C\n", (double)body_c);
		}

		if (vitaband_health_notify_enabled()) {
			uint8_t risk = calculate_risk_score(last_body_c, base_skin_c, HR_BPM_FIXED,
							      HR_BPM_FIXED);

			err = vitaband_health_notify(HR_BPM_FIXED, last_body_c, AMBIENT_C_FIXED, OK,
						     risk);
			if (err != 0) {
				printk("ble-body: notify err %d\n", err);
			}
		}

		k_sleep(K_SECONDS(1));
	}
}
