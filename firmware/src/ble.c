/*
 * VitaBand BLE: custom GATT health telemetry (read + notify).
 */

#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>

#include "ble.h"
#include "state_manager.h"

static const struct bt_uuid_128 vitaband_svc_uuid =
	BT_UUID_INIT_128(VITABAND_HEALTH_SVC_UUID_VAL);
static const struct bt_uuid_128 vitaband_chr_telemetry_uuid =
	BT_UUID_INIT_128(VITABAND_HEALTH_CHR_TELEMETRY_UUID_VAL);
static const struct bt_uuid_128 vitaband_chr_calibration_uuid =
    BT_UUID_INIT_128(VITABAND_HEALTH_CHR_CALIBRATION_UUID_VAL);

static uint8_t last_payload[VITABAND_HEALTH_NOTIFY_PAYLOAD_LEN];
static bool    notify_enabled;

static void encode_payload(uint8_t *d, uint8_t hr_bpm, float body_c, float amb_c,
			   vitaband_state_t state, uint32_t uptime_ms)
{
	d[0] = hr_bpm;
	memcpy(&d[1], &body_c, sizeof(float));
	memcpy(&d[5], &amb_c, sizeof(float));
	d[9] = (uint8_t)state;
	memcpy(&d[10], &uptime_ms, sizeof(uint32_t));
}

static ssize_t read_telemetry(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
			      uint16_t len, uint16_t offset)
{
	ARG_UNUSED(attr);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, last_payload,
				 sizeof(last_payload));
}

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}
static ssize_t write_calibration(struct bt_conn *conn,
                                 const struct bt_gatt_attr *attr,
                                 const void *buf,
                                 uint16_t len,
                                 uint16_t offset,
                                 uint8_t flags)
{
    ARG_UNUSED(conn);
    ARG_UNUSED(attr);
    ARG_UNUSED(flags);

    if (offset != 0 || len != 5) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const uint8_t *data = buf;

    uint8_t baseline_hr = data[0];

    float baseline_skin_temp_c;
    memcpy(&baseline_skin_temp_c, &data[1], sizeof(float));

    state_manager_set_baseline(baseline_skin_temp_c, baseline_hr);

    return len;
}

BT_GATT_SERVICE_DEFINE(ble_health_svc,
    BT_GATT_PRIMARY_SERVICE(&vitaband_svc_uuid),

    BT_GATT_CHARACTERISTIC(&vitaband_chr_telemetry_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_telemetry,
                           NULL,
                           NULL),

    BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(&vitaband_chr_calibration_uuid.uuid,
                           BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE,
                           NULL,
                           write_calibration,
                           NULL)
);
bool vitaband_health_notify_enabled(void)
{
    return notify_enabled;
}

int vitaband_health_notify(uint8_t hr_bpm, float body_temp_c, float ambient_temp_c,
                           vitaband_state_t state)
{
    if (!notify_enabled) {
        return 0;
    }

    uint32_t uptime_ms = k_uptime_get_32();

    encode_payload(last_payload, hr_bpm, body_temp_c, ambient_temp_c, state, uptime_ms);

    return bt_gatt_notify(NULL, &ble_health_svc.attrs[2], last_payload,
                          sizeof(last_payload));
}
