/*
 * Bluetooth LE: custom GATT health service (telemetry notify) + UUIDs for advertising.
 * GATT = Generic Attribute Profile (BLE services/characteristics/descriptors).
 */

#ifndef BLE_H
#define BLE_H

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/bluetooth/uuid.h>

#include <state_manager.h>

/* 128-bit UUIDs (Zephyr BT_UUID_128_ENCODE byte order) */
#define VITABAND_HEALTH_SVC_UUID_VAL                                                       \
	BT_UUID_128_ENCODE(0x8b4cb001, 0x7a2e, 0x4c91, 0xb3d6, 0x1c0de5a1b2c3)

#define VITABAND_HEALTH_CHR_TELEMETRY_UUID_VAL                                             \
	BT_UUID_128_ENCODE(0x8b4cb002, 0x7a2e, 0x4c91, 0xb3d6, 0x1c0de5a1b2c3)

/*
 * Canonical UUID strings (CoreBluetooth / nRF Connect):
 *   Service:    8b4cb001-7a2e-4c91-b3d6-1c0de5a1b2c3
 *   Telemetry:  8b4cb002-7a2e-4c91-b3d6-1c0de5a1b2c3
 */

#define VITABAND_HEALTH_STATE_OK        0U
#define VITABAND_HEALTH_STATE_WARNING   1U
#define VITABAND_HEALTH_STATE_CRITICAL  2U
#define VITABAND_HEALTH_STATE_EMERGENCY   3U

#define VITABAND_HEALTH_NOTIFY_PAYLOAD_LEN 15U

/*
 * Notification payload (15 octets, little-endian):
 *   offset 0:       uint8  heart rate (bpm)
 *   offset 1..4:    float  body (skin) temperature (°C)
 *   offset 5..8:    float  ambient temperature (°C)
 *   offset 9:       uint8  state (0=OK, 1=WARNING, 2=CRITICAL, 3=EMERGENCY)
 *   offset 10..13:  uint32 uptime since boot (ms)
 *   offset 14:      uint8  risk score PSI 0..10 from calculate_risk_score (0 if unknown)
 */

bool vitaband_health_notify_enabled(void);

/** @brief Notify subscribed centrals; no-op if CCC notify not enabled. */
int vitaband_health_notify(uint8_t hr_bpm, float body_temp_c, float ambient_temp_c,
			   vitaband_state_t state, uint8_t risk_score);

#endif /* BLE_H */
