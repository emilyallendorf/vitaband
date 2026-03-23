/*
 * MAX86140 Heart Rate / PPG Sensor Driver
 * SPI-based implementation for Zephyr RTOS
 * 
 *SPI interface, 19-bit ADC, 1.8V operation
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include "max86140.h"

LOG_MODULE_REGISTER(max86140, LOG_LEVEL_INF);


/* ========================================================================== */
/* REGISTER DEFINITIONS                                                       */
/* ========================================================================== */

/* Status & Interrupt Registers */
#define REG_INT_STATUS_1        0x00
#define REG_INT_STATUS_2        0x01
#define REG_INT_ENABLE_1        0x02
#define REG_INT_ENABLE_2        0x03

/* FIFO Registers */
#define REG_FIFO_WRITE_PTR      0x04
#define REG_FIFO_READ_PTR       0x05
#define REG_FIFO_OVERFLOW       0x06
#define REG_FIFO_DATA_COUNT     0x07
#define REG_FIFO_DATA_REG       0x08
#define REG_FIFO_CONFIG_1       0x09
#define REG_FIFO_CONFIG_2       0x0A

/* System Control */
#define REG_SYSTEM_CONTROL      0x0D
#define REG_PPG_SYNC_CONTROL    0x10
#define REG_PPG_CONFIG_1        0x11
#define REG_PPG_CONFIG_2        0x12
#define REG_PPG_CONFIG_3        0x13

/* LED Configuration */
#define REG_LED_SEQ_1           0x20
#define REG_LED_SEQ_2           0x21
#define REG_LED_SEQ_3           0x22
#define REG_LED1_PA             0x23
#define REG_LED2_PA             0x24
#define REG_LED3_PA             0x25

/* Part ID */
#define REG_PART_ID             0xFF
#define MAX86140_PART_ID        0x24  /* Expected value for MAX86140 */

/* SPI Commands */
#define SPI_WRITE_CMD           0x00
#define SPI_READ_CMD            0xFF

/* ========================================================================== */
/* DEVICE CONFIGURATION                                                       */
/* ========================================================================== */

static const struct device *spi_dev;
static struct spi_config spi_cfg;
static struct spi_cs_control cs_ctrl;

/* Get device from devicetree */
#define MAX86140_NODE DT_NODELABEL(max86140)

static const struct spi_dt_spec max86140_spi = SPI_DT_SPEC_GET(
    MAX86140_NODE,
    SPI_OP_MODE_MASTER | SPI_WORD_SET(8),
    0
);


int max86140_init(void) {
    if (!spi_is_ready_dt(&max86140_spi)) {
        LOG_ERR("SPI bus for MAX86140 not ready");
        return -ENODEV;
    }
    
    LOG_INF("MAX86140 SPI interface initialized.");
    return 0;
}
/* ========================================================================== */
/* SPI COMMUNICATION                                                          */
/* ========================================================================== */

/**
 * MAX86140 SPI Protocol:
 * - 3-byte transactions (24 clock cycles)
 * - Byte 1: Register address
 * - Byte 2: Command (0x00 = write, 0xFF = read)
 * - Byte 3: Data (write) or received data (read)
 * - Data strobed in on SCLK rising edge
 * - Data strobed out on SCLK falling edge
 */

static int max86140_reg_write(uint8_t reg, uint8_t value)
{
    uint8_t tx_buf[3] = {reg, SPI_WRITE_CMD, value};
    uint8_t rx_buf[3] = {0};
    
    const struct spi_buf tx = {.buf = tx_buf, .len = 3};
    const struct spi_buf rx = {.buf = rx_buf, .len = 3};
    const struct spi_buf_set tx_set = {.buffers = &tx, .count = 1};
    const struct spi_buf_set rx_set = {.buffers = &rx, .count = 1};
    
    int ret = spi_transceive_dt(&max86140_spi, &tx_set, &rx_set);
    if (ret != 0) {
        LOG_ERR("SPI write failed: %d", ret);
        return ret;
    }
    
    LOG_DBG("Write: reg=0x%02X, val=0x%02X", reg, value);
    return 0;
}

static int max86140_reg_read(uint8_t reg, uint8_t *value)
{
    uint8_t tx_buf[3] = {reg, SPI_READ_CMD, 0x00};
    uint8_t rx_buf[3] = {0};
    
    const struct spi_buf tx = {.buf = tx_buf, .len = 3};
    const struct spi_buf rx = {.buf = rx_buf, .len = 3};
    const struct spi_buf_set tx_set = {.buffers = &tx, .count = 1};
    const struct spi_buf_set rx_set = {.buffers = &rx, .count = 1};
    
    int ret = spi_transceive_dt(&max86140_spi, &tx_set, &rx_set);
    if (ret != 0) {
        LOG_ERR("SPI read failed: %d", ret);
        return ret;
    }
    
    *value = rx_buf[2];  /* Data is in third byte */
    LOG_DBG("Read: reg=0x%02X, val=0x%02X", reg, *value);
    
    return 0;
}

/* ========================================================================== */
/* INITIALIZATION                                                             */
/* ========================================================================== */

// int max86140_init(void)
// {
//     int ret;
//     uint8_t part_id;
    
//     LOG_INF("Initializing MAX86140");
    
//     /* Check if SPI device is ready */
//     if (!spi_is_ready_dt(&max86140_spi)) {
//         LOG_ERR("SPI device not ready");
//         return -ENODEV;
//     }
    
//     /* Verify Part ID */
//     ret = max86140_reg_read(REG_PART_ID, &part_id);
//     if (ret != 0) {
//         LOG_ERR("Failed to read Part ID");
//         return ret;
//     }
    
//     if (part_id != MAX86140_PART_ID) {
//         LOG_WRN("Unexpected Part ID: 0x%02X (expected 0x%02X)", 
//                 part_id, MAX86140_PART_ID);
//         /* Continue anyway - might still work */
//     } else {
//         LOG_INF("MAX86140 detected (Part ID: 0x%02X)", part_id);
//     }
    
//     /* Reset device */
//     ret = max86140_reg_write(REG_SYSTEM_CONTROL, 0x01);  /* SW_RST bit */
//     if (ret != 0) {
//         LOG_ERR("Failed to reset device");
//         return ret;
//     }
    
//     k_msleep(100);  /* Wait for reset */
    
//     /* Configure FIFO */
//     ret = max86140_reg_write(REG_FIFO_CONFIG_1, 0x10);  /* Almost full = 16 */
//     if (ret != 0) {
//         LOG_ERR("Failed to configure FIFO");
//         return ret;
//     }
    
//     /* TODO: Add more detailed configuration:
//      * - PPG sample rate
//      * - LED current
//      * - LED sequence
//      * - ADC range
//      * - etc.
//      */
    
//     LOG_INF("MAX86140 initialized successfully");
//     return 0;
// }

/* ========================================================================== */
/* DATA READING (PLACEHOLDER - needs full implementation)                    */
/* ========================================================================== */

uint8_t max86140_read_heartrate(void)
{
    /* TODO: Implement full FIFO reading and HR calculation
     * This is a placeholder for now
     * 
     * The MAX86140 is much more complex than MAX30102:
     * - Needs proper PPG configuration
     * - FIFO data is 19-bit, not 18-bit
     * - Multiple LED channels
     * - Advanced filtering needed
     */
    
    static uint8_t placeholder_hr = 72;
    
    LOG_WRN("MAX86140 driver incomplete - returning placeholder HR");
    return placeholder_hr;
}

/* ========================================================================== */
/* DIAGNOSTICS                                                                */
/* ========================================================================== */

void max86140_print_status(void)
{
    uint8_t part_id, fifo_wr, fifo_rd, fifo_count;
    
    max86140_reg_read(REG_PART_ID, &part_id);
    max86140_reg_read(REG_FIFO_WRITE_PTR, &fifo_wr);
    max86140_reg_read(REG_FIFO_READ_PTR, &fifo_rd);
    max86140_reg_read(REG_FIFO_DATA_COUNT, &fifo_count);
    
    LOG_INF("=== MAX86140 Status ===");
    LOG_INF("Part ID: 0x%02X", part_id);
    LOG_INF("FIFO Write Ptr: %d", fifo_wr);
    LOG_INF("FIFO Read Ptr: %d", fifo_rd);
    LOG_INF("FIFO Count: %d", fifo_count);
}