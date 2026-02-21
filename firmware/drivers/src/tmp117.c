#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include "tmp117.h"


/* ========================== REGISTER DEFINITIONS ========================== */

/* ========================== CONSTANTS & BUFFERS =========================== */

// Get the device reference from the devicetree
static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(tmp117));

/* =========================== HELPER FUNCTIONS ============================= */
static int tmp117_write_reg(uint8_t reg, uint8_t value){
    return i2c_reg_write_byte_dt(&i2c_dev, reg, value);
}

static int tmp117_read_reg(uint8_t reg, uint8_t *value){
    return i2c_reg_read_byte_dt(&i2c_dev, reg, value);
}

/* ============================ INITIALIZATION ============================== */
int tmp117_init(void) {
    if (!device_is_ready(i2c_dev.bus)) {
        LOG_ERR("i2c bus not ready");
        return -ENODEV;}

        // TODO
    }

/* ============================ DATA COLLECTION ============================= */

/* ============================== PUBLIC API ================================ */
uint8_t tmp117_read_temperature(void) {} // TODO