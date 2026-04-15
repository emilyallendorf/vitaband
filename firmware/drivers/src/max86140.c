#include "max86140.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(max86140, LOG_LEVEL_INF);

/* ══════════════════════════════════════════════════════════════════════════
 * REGISTER MAP  (MAX86140/MAX86141 datasheet Rev.5)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Status / Interrupt */
#define REG_INT_STATUS_1    0x00
#define REG_INT_STATUS_2    0x01
#define REG_INT_ENABLE_1    0x02
#define REG_INT_ENABLE_2    0x03

/* FIFO */
#define REG_FIFO_WR_PTR     0x04
#define REG_FIFO_RD_PTR     0x05
#define REG_OVF_COUNTER     0x06
#define REG_FIFO_DATA_COUNT 0x07
#define REG_FIFO_DATA       0x08
#define REG_FIFO_CONFIG_1   0x09   /* FIFO_A_FULL threshold */
#define REG_FIFO_CONFIG_2   0x0A   /* FLUSH_FIFO, FIFO_STAT_CLR, A_FULL_TYPE, FIFO_ROLL_ON_FULL */

/* System Control */
#define REG_SYSTEM_CTRL     0x0D   /* RESET[0], SHDN[1] */

/* PPG Sync / Config */
#define REG_PPG_SYNC_CTRL   0x10
#define REG_PPG_CONFIG_1    0x11   /* PPG_ADC_RGE, PPG_TINT, LP_MODE */
#define REG_PPG_CONFIG_2    0x12   /* PPG_SR[4:0], PPG_LED_SETLNG[1:0] */
#define REG_PPG_CONFIG_3    0x13   /* PD_BIAS */

/* LED Sequence Control (which LED fires each slot) */
#define REG_LED_SEQ_1       0x20   /* LEDC1[3:0], LEDC2[3:0] */
#define REG_LED_SEQ_2       0x21   /* LEDC3[3:0], LEDC4[3:0] */
#define REG_LED_SEQ_3       0x22   /* LEDC5[3:0], LEDC6[3:0] */

/* LED Pulse Amplitude */
#define REG_LED1_PA         0x23
#define REG_LED2_PA         0x24
#define REG_LED3_PA         0x25

/* LED Range */
#define REG_LED_RANGE_1     0x2A   /* LED1_RGE[1:0], LED2_RGE[1:0], LED3_RGE[1:0] */

/* Die Temperature */
#define REG_TEMP_CONFIG     0x40
#define REG_TEMP_INT        0x41
#define REG_TEMP_FRAC       0x42

/* Part / Revision ID */
#define REG_REV_ID          0xFE
#define REG_PART_ID         0xFF

/* ── Interrupt status bit masks (REG_INT_STATUS_1) ── */
#define INT_A_FULL          (1 << 7)  /* FIFO almost full */
#define INT_PPG_RDY         (1 << 6)  /* New FIFO data ready */
#define INT_ALC_OVF         (1 << 5)  /* Ambient light overflow */
#define INT_PROX_INT        (1 << 4)  /* Proximity threshold crossed */
#define INT_LED_COMPA       (1 << 3)  /* LED compliance flag */
#define INT_DIE_TEMP_RDY    (1 << 2)  /* Die temp conversion done */
#define INT_PWR_RDY         (1 << 0)  /* Power ready */

/* ── FIFO config 2 bits ── */
#define FIFO_FLUSH          (1 << 4)
#define FIFO_STAT_CLR       (1 << 3)
#define FIFO_ROLL_ON_FULL   (1 << 1)

/* ── System control ── */
#define SYS_RESET           (1 << 0)
#define SYS_SHDN            (1 << 1)

/* ── PPG Config 1 ──
 *   [7:6] PPG1_ADC_RGE   0=4μA 1=8μA 2=16μA 3=32μA
 *   [5:4] PPG_TINT       0=14.8μs 1=29.4μs 2=58.7μs 3=117.3μs
 *   [1]   LP_MODE        1=low-power
 */
#define PPG_ADC_RGE_16UA    (0x2 << 6)
#define PPG_TINT_117US      (0x3 << 4)
#define PPG_LP_MODE         (1 << 1)

/* ── PPG Config 2 ──
 *   [7:3] PPG_SR         sample rate code (see Table in datasheet)
 *         0x07 = 100 sps, 0x09 = 200 sps
 *   [1:0] LED_SETLNG     settling time 0=6μs 1=6μs 2=12μs 3=12μs
 */
#define PPG_SR_100SPS       (0x07 << 3)
#define PPG_LED_SETLNG_6US  (0x00)

/* ── LED sequence codes (LEDC register values) ──
 *   0x0 = none, 0x1 = LED1, 0x2 = LED2, 0x9 = direct ambient
 */
#define LEDC_NONE           0x0
#define LEDC_LED1           0x1
#define LEDC_LED2           0x2
#define LEDC_DIRECT_AMB     0x9

/* Expected part ID */
#define MAX86140_PART_ID    0x24

/* ══════════════════════════════════════════════════════════════════════════
 * SPI FRAMING
 *
 * Every SPI transaction has a 2-byte header:
 *   Byte 0:  reg_addr[6:0] | R/W   (R=1, W=0)
 *   Byte 1:  dummy byte for read / data byte for single-byte write
 *
 * FIFO burst read appends N*3 data bytes after the 2-byte header.
 * The nRF52840 SPIM can handle this in one transfer.
 * ══════════════════════════════════════════════════════════════════════════ */
#define SPI_READ_BIT        0x80
#define SPI_WRITE_BIT       0x00

/* ══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ══════════════════════════════════════════════════════════════════════════ */
#define SAMPLES_PER_READ    32
#define BYTES_PER_SAMPLE    3      /* 19-bit left-justified + 3-bit tag */
#define MIN_HEART_RATE      40
#define MAX_HEART_RATE      220
#define SAMPLE_RATE_HZ      100

/*
 * FIFO data format (datasheet Table 6):
 *   Byte 0: [D[18:11]]
 *   Byte 1: [D[10:3]]
 *   Byte 2: [D[2:0] | TAG[2:0]] — lower 3 bits are the FIFO tag
 * Data is left-justified → shift right by 3 to get the 19-bit ADC value.
 */
#define FIFO_DATA_MASK      0x7FFFF8UL   /* bits [23:3] → 19 bits after >>3 */

/* ══════════════════════════════════════════════════════════════════════════
 * SPI DEVICE — from devicetree
 * ══════════════════════════════════════════════════════════════════════════ */
static const struct spi_dt_spec spi_dev =
    SPI_DT_SPEC_GET(DT_NODELABEL(max86140),
                    SPI_OP_MODE_MASTER |
                    SPI_WORD_SET(8)    |
                    SPI_TRANSFER_MSB,
                    0);

/* ══════════════════════════════════════════════════════════════════════════
 * STATIC BUFFERS
 * ══════════════════════════════════════════════════════════════════════════ */
/* Worst-case: 2-byte header + 32 samples * 3 bytes = 98 bytes */
#define TX_BUF_LEN   (2 + SAMPLES_PER_READ * BYTES_PER_SAMPLE)
#define RX_BUF_LEN   (2 + SAMPLES_PER_READ * BYTES_PER_SAMPLE)

static uint8_t tx_buf[TX_BUF_LEN];
static uint8_t rx_buf[RX_BUF_LEN];

static int32_t ir_samples[SAMPLES_PER_READ];

/* Heart rate state */
static uint8_t  calculated_hr   = 0;
static uint32_t last_peak_time  = 0;

/* ══════════════════════════════════════════════════════════════════════════
 * LOW-LEVEL SPI HELPERS
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write a single byte to a register.
 */
static int max86140_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { reg & ~SPI_READ_BIT, val };

    struct spi_buf tx_spi = { .buf = tx, .len = 2 };
    struct spi_buf_set tx_set = { .buffers = &tx_spi, .count = 1 };

    return spi_write_dt(&spi_dev, &tx_set);
}

/**
 * @brief Read a single byte from a register.
 */
static int max86140_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = { reg | SPI_READ_BIT, 0x00 };
    uint8_t rx[2] = { 0 };

    struct spi_buf tx_spi = { .buf = tx, .len = 2 };
    struct spi_buf rx_spi = { .buf = rx, .len = 2 };
    struct spi_buf_set tx_set = { .buffers = &tx_spi, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_spi, .count = 1 };

    int ret = spi_transceive_dt(&spi_dev, &tx_set, &rx_set);
    if (ret == 0) {
        *val = rx[1];   /* data comes back in second byte */
    }
    return ret;
}

/**
 * @brief Burst-read N bytes from REG_FIFO_DATA.
 *
 * Header (2 bytes) + data (n_bytes) sent/received in a single SPI transfer.
 * The nRF52840 SPIM handles up to 65535 bytes per transfer.
 */
static int max86140_read_fifo_burst(uint8_t n_samples, uint8_t *data_out)
{
    uint32_t total = 2 + (uint32_t)n_samples * BYTES_PER_SAMPLE;

    memset(tx_buf, 0, total);
    tx_buf[0] = REG_FIFO_DATA | SPI_READ_BIT;
    tx_buf[1] = 0x00;   /* dummy */

    struct spi_buf tx_spi = { .buf = tx_buf, .len = total };
    struct spi_buf rx_spi = { .buf = rx_buf, .len = total };
    struct spi_buf_set tx_set = { .buffers = &tx_spi, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_spi, .count = 1 };

    int ret = spi_transceive_dt(&spi_dev, &tx_set, &rx_set);
    if (ret == 0) {
        /* Skip the 2-byte header in rx_buf */
        memcpy(data_out, &rx_buf[2], n_samples * BYTES_PER_SAMPLE);
    }
    return ret;
}

/* ══════════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ══════════════════════════════════════════════════════════════════════════ */

int max86140_init(void)
{
    if (!spi_is_ready_dt(&spi_dev)) {
        LOG_ERR("SPI bus not ready for MAX86140");
        return -ENODEV;
    }

    /* ── 1. Read and verify Part ID ── */
    uint8_t part_id = 0;
    int ret = max86140_read_reg(REG_PART_ID, &part_id);
    if (ret != 0) {
        LOG_ERR("Failed to read Part ID: %d", ret);
        return ret;
    }
    if (part_id != MAX86140_PART_ID) {
        LOG_WRN("Unexpected Part ID: 0x%02x (expected 0x%02x)",
                part_id, MAX86140_PART_ID);
    } else {
        LOG_INF("MAX86140 detected. Part ID: 0x%02x", part_id);
    }

    /* ── 2. Software reset ── */
    ret = max86140_write_reg(REG_SYSTEM_CTRL, SYS_RESET);
    if (ret != 0) {
        LOG_ERR("Reset failed: %d", ret);
        return ret;
    }
    k_msleep(10);   /* datasheet: allow POR to complete */

    /* ── 3. Configure FIFO ──
     *   FIFO_CONFIG_1: A_FULL threshold = 0x10 (interrupt at 112 samples)
     *   FIFO_CONFIG_2: FIFO_ROLL_ON_FULL enabled, stat cleared on read
     */
    ret = max86140_write_reg(REG_FIFO_CONFIG_1, 0x10);
    if (ret != 0) { LOG_ERR("FIFO config 1 failed: %d", ret); return ret; }

    ret = max86140_write_reg(REG_FIFO_CONFIG_2,
                             FIFO_ROLL_ON_FULL | FIFO_STAT_CLR);
    if (ret != 0) { LOG_ERR("FIFO config 2 failed: %d", ret); return ret; }

    /* ── 4. Configure PPG ──
     *   ADC range: 16μA, integration time: 117.3μs, LP_MODE on
     *   Sample rate: 100 sps, LED settling: 6μs
     */
    ret = max86140_write_reg(REG_PPG_CONFIG_1,
                             PPG_ADC_RGE_16UA | PPG_TINT_117US | PPG_LP_MODE);
    if (ret != 0) { LOG_ERR("PPG config 1 failed: %d", ret); return ret; }

    ret = max86140_write_reg(REG_PPG_CONFIG_2,
                             PPG_SR_100SPS | PPG_LED_SETLNG_6US);
    if (ret != 0) { LOG_ERR("PPG config 2 failed: %d", ret); return ret; }

    /* ── 5. Configure LED sequence ──
     *   Slot 1: LED1 (IR)
     *   Slot 2: LED2 (Red)  — useful if you want SpO2 later
     *   Slots 3-6: disabled
     *
     *   REG_LED_SEQ_1 = [LEDC2 | LEDC1] = [LED2 << 4 | LED1]
     */
    ret = max86140_write_reg(REG_LED_SEQ_1,
                             (LEDC_LED2 << 4) | LEDC_LED1);
    if (ret != 0) { LOG_ERR("LED seq 1 failed: %d", ret); return ret; }

    ret = max86140_write_reg(REG_LED_SEQ_2, 0x00);  /* slots 3+4 off */
    if (ret != 0) { LOG_ERR("LED seq 2 failed: %d", ret); return ret; }

    ret = max86140_write_reg(REG_LED_SEQ_3, 0x00);  /* slots 5+6 off */
    if (ret != 0) { LOG_ERR("LED seq 3 failed: %d", ret); return ret; }

    /* ── 6. Set LED pulse amplitudes ~~25 mA each ──
     *   With LEDx_RGE = 0x0 (31 mA full scale), 0x7F ≈ 15.5 mA
     */
    ret = max86140_write_reg(REG_LED1_PA, 0x7F);
    if (ret != 0) { LOG_ERR("LED1 PA failed: %d", ret); return ret; }

    ret = max86140_write_reg(REG_LED2_PA, 0x7F);
    if (ret != 0) { LOG_ERR("LED2 PA failed: %d", ret); return ret; }

    /* ── 7. Enable interrupts: A_FULL and PPG_RDY ── */
    ret = max86140_write_reg(REG_INT_ENABLE_1, INT_A_FULL | INT_PPG_RDY);
    if (ret != 0) { LOG_ERR("INT enable failed: %d", ret); return ret; }

    /* ── 8. Clear any pending interrupt flags ── */
    uint8_t dummy;
    max86140_read_reg(REG_INT_STATUS_1, &dummy);
    max86140_read_reg(REG_INT_STATUS_2, &dummy);

    /* ── 9. Flush FIFO before starting ── */
    ret = max86140_write_reg(REG_FIFO_CONFIG_2,
                             FIFO_FLUSH | FIFO_ROLL_ON_FULL | FIFO_STAT_CLR);
    if (ret != 0) { LOG_ERR("FIFO flush failed: %d", ret); return ret; }

    /* Re-arm FIFO config without flush bit */
    ret = max86140_write_reg(REG_FIFO_CONFIG_2,
                             FIFO_ROLL_ON_FULL | FIFO_STAT_CLR);
    if (ret != 0) { LOG_ERR("FIFO re-arm failed: %d", ret); return ret; }

    /* ── 10. Exit shutdown ── */
    ret = max86140_write_reg(REG_SYSTEM_CTRL, 0x00);
    if (ret != 0) { LOG_ERR("Exit shutdown failed: %d", ret); return ret; }

    LOG_INF("MAX86140 initialized successfully");
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * FIFO READ
 * ══════════════════════════════════════════════════════════════════════════ */

static int max86140_read_fifo(uint8_t *sample_count_out, int32_t *ir_out)
{
    uint8_t fifo_count = 0;
    int ret = max86140_read_reg(REG_FIFO_DATA_COUNT, &fifo_count);
    if (ret != 0) {
        LOG_ERR("FIFO count read failed: %d", ret);
        return ret;
    }
    if (fifo_count == 0) {
        *sample_count_out = 0;
        return 0;
    }

    uint8_t n = (fifo_count > SAMPLES_PER_READ) ? SAMPLES_PER_READ : fifo_count;

    /* Raw FIFO bytes: n samples × 2 channels × 3 bytes each
     * We configured 2 LED slots (LED1=IR, LED2=Red), so each "frame"
     * in the FIFO is 2 × 3 = 6 bytes. We only extract the IR channel (slot 1). */
    static uint8_t raw[SAMPLES_PER_READ * BYTES_PER_SAMPLE * 2];
    ret = max86140_read_fifo_burst(n * 2, raw);   /* 2 channels per sample */
    if (ret != 0) {
        LOG_ERR("FIFO burst read failed: %d", ret);
        return ret;
    }

    /*
     * Parse FIFO data.
     * Format per sample (3 bytes, left-justified 19-bit):
     *   raw[0] = D[18:11]
     *   raw[1] = D[10:3]
     *   raw[2] = D[2:0] | TAG[2:0]
     *
     * Extract the 19-bit value by shifting the 3 bytes into a 32-bit word
     * and masking out the tag bits. Data is left-justified so we shift >>3.
     */
    for (int i = 0; i < n; i++) {
        /* Slot 1 (IR) is the first of each pair */
        int base = i * 6;
        uint32_t raw32 = ((uint32_t)raw[base + 0] << 16) |
                         ((uint32_t)raw[base + 1] << 8)  |
                         ((uint32_t)raw[base + 2]);
        ir_out[i] = (int32_t)((raw32 >> 3) & 0x0007FFFF);   /* 19-bit value */
    }

    *sample_count_out = n;
    LOG_DBG("Read %u samples from FIFO", n);
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * HEART RATE CALCULATION
 *
 * Simple peak-detection on the IR channel. Identical algorithm to the
 * MAX30102 driver but adapted for MAX86140's 19-bit ADC range.
 * ══════════════════════════════════════════════════════════════════════════ */

static uint8_t max86140_calculate_hr(int32_t *samples, uint8_t n)
{
    static int32_t baseline          = 0;
    static bool    baseline_ready    = false;

    if (n == 0) return calculated_hr;

    /* Compute mean of this batch */
    int64_t sum = 0;
    for (int i = 0; i < n; i++) sum += samples[i];
    int32_t batch_mean = (int32_t)(sum / n);

    if (!baseline_ready) {
        baseline       = batch_mean;
        baseline_ready = true;
        return 0;
    }

    /* Slow-tracking baseline (95% old + 5% new) */
    baseline = (baseline * 95 + batch_mean * 5) / 100;

    /* Threshold: 10% above baseline */
    int32_t threshold = baseline + (baseline / 10);

    uint32_t now_ms = k_uptime_get_32();

    for (int i = 1; i < n - 1; i++) {
        bool is_peak = (samples[i] > samples[i - 1]) &&
                       (samples[i] > samples[i + 1]) &&
                       (samples[i] > threshold);

        if (is_peak && last_peak_time > 0) {
            uint32_t interval_ms = now_ms - last_peak_time;
            if (interval_ms > 0) {
                uint32_t bpm = 60000U / interval_ms;
                if (bpm >= MIN_HEART_RATE && bpm <= MAX_HEART_RATE) {
                    calculated_hr = (uint8_t)bpm;
                    LOG_DBG("HR: %u BPM (interval %u ms)", calculated_hr, interval_ms);
                }
            }
            last_peak_time = now_ms;
        } else if (is_peak) {
            last_peak_time = now_ms;
        }
    }

    return calculated_hr;
}

/* ══════════════════════════════════════════════════════════════════════════
 * PUBLIC API
 * ══════════════════════════════════════════════════════════════════════════ */

uint8_t max86140_read_heartrate(void)
{
    uint8_t n = 0;
    int ret = max86140_read_fifo(&n, ir_samples);
    if (ret != 0 || n == 0) {
        return calculated_hr;   /* return last known value */
    }
    return max86140_calculate_hr(ir_samples, n);
}