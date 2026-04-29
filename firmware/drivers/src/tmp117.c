#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include "tmp117.h"
#include <config.h>

LOG_MODULE_REGISTER(tmp117, LOG_LEVEL_DBG);

#if IS_ENABLED(CONFIG_VITABAND_BLE_HR_BODY_TEST)
#undef LOG_DBG
#undef LOG_WRN
#undef LOG_ERR
#define LOG_DBG(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#endif

/* ========================== REGISTER DEFINITIONS ========================== */
#define REG_TEMP_RESULT     0x00
#define REG_CONFIGURATION   0x01
#define REG_THIGH_LIMIT     0x02
#define REG_TLOW_LIMIT      0x03
#define REG_DEVICE_ID       0x0F
/* Configuration Bits */
#define CONV_MODE_CONTINUOUS (0 << 10)
#define CONV_MODE_SHUTDOWN   (1 << 10)
#define CONV_MODE_ONESHOT    (3 << 10)

/* ========================== CONSTANTS & BUFFERS =========================== */
#define TMP117_DEVICE_ID_VALUE 0x0117
#define NO_TEMP -99.0f

// Get the device reference from the devicetree
static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(tmp117));

/* =========================== HELPER FUNCTIONS ============================= */
/**
 * @brief Writes a 16-bit value to a TMP117 register.
 * TMP117 expects Big-Endian
 */
static int tmp117_write_reg16(uint8_t reg, uint16_t value) {
    uint8_t buf[3];
    buf[0] = reg;
    buf[1] = (value >> 8) & 0xFF; // MSB
    buf[2] = value & 0xFF;        // LSB
    return i2c_write_dt(&i2c_dev, buf, sizeof(buf));
}

/**
 * @brief Reads a 16-bit value from a TMP117 register.
 */
static int tmp117_read_reg16(uint8_t reg, uint16_t *value) {
    uint8_t buf[2];
    int ret = i2c_write_read_dt(&i2c_dev, &reg, 1, buf, sizeof(buf));
    if (ret == 0) {
        *value = (buf[0] << 8) | buf[1]; // Combine MSB and LSB
    }
    return ret;
}

/* ============================ INITIALIZATION ============================== */
int tmp117_init(void) {
    if (!device_is_ready(i2c_dev.bus)) {
        LOG_ERR("TMP117: I2C bus not ready");
        return -ENODEV;
    }

    uint16_t device_id;
    int ret = tmp117_read_reg16(REG_DEVICE_ID, &device_id);
    if (ret != 0) {
        LOG_ERR("Failed to read TMP117 Device ID: %d", ret);
        return ret;
    }

    if (device_id != TMP117_DEVICE_ID_VALUE) {
        LOG_WRN("Unexpected TMP117 ID: 0x%04x (expected 0x0117)", device_id);
    }

    /* Continuous conversion mode */
    ret = tmp117_write_reg16(REG_CONFIGURATION, 0x0220);
    if (ret != 0) {
        LOG_ERR("Failed to configure TMP117: %d", ret);
        return ret;
    }

    LOG_DBG("TMP117 initialized. ID: 0x%04x", device_id);
    return 0;
}

/* ============================ DATA COLLECTION ============================= */
/**
 * @brief Internal function to get the raw 16-bit temperature.
 */
static int tmp117_get_raw_temp(int16_t *raw_temp) {
    uint16_t val;
    int ret = tmp117_read_reg16(REG_TEMP_RESULT, &val);
    if (ret == 0) *raw_temp = (int16_t)val;
    return ret;
}



/* ============================== PUBLIC API ================================ */
float tmp117_read_temperature(void) {
    int16_t raw_temp;
    int ret = tmp117_get_raw_temp(&raw_temp);
    
    if (ret != 0) {
        LOG_ERR("Could not read temperature");
        return NO_TEMP;
    }

    /* Conversion logic:
     * Resolution is 0.0078125°C per LSB.
     * Temp (°C) = Raw_Data * 0.0078125 */
    float temp_c = (float)raw_temp * 0.0078125f;

    return temp_c; 
}