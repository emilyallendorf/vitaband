#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include "max30102.h"

LOG_MODULE_REGISTER(max30102, LOG_LEVEL_INF);

// TODO: check all of the register defines against the datasheet
/* ========================== REGISTER DEFINITIONS ========================== */
/* Status Registers */
#define REG_INT_STATUS_1    0x00
#define REG_INT_STATUS_2    0x01
#define REG_INT_ENABLE_1    0x02
#define REG_INT_ENABLE_2    0x03
/* FIFO Registers */
#define REG_FIFO_WR_PTR     0x04
#define REG_FIFO_RD_PTR     0x05
#define REG_OVF_COUNTER     0x06
#define REG_FIFO_DATA_COUNT 0x07
#define REG_FIFO_DATA       0x08
#define REG_FIFO_CONFIG_1   0x09
#define REG_FIFO_CONFIG_2   0x0A
/* System Control */
#define REG_SYSTEM_CONTROL  0x0D
#define REG_PPG_SYNC_CONTROL 0x10
#define REG_PPG_CONFIG_1    0x11
#define REG_PPG_CONFIG_2    0x12
#define REG_PPG_CONFIG_3    0x13
/* Proximity */
#define REG_PROX_INT_THRESH 0x14
/* LED Configuration */
#define REG_LED_SEQ_1       0x20
#define REG_LED_SEQ_2       0x21
#define REG_LED_SEQ_3       0x22
#define REG_LED1_PA         0x23
#define REG_LED2_PA         0x24
#define REG_LED3_PA         0x25
#define REG_PILOT_PA        0x29
/* Die Temperature */
#define REG_DIE_TEMP_INT    0x40
#define REG_DIE_TEMP_FRAC   0x41
#define REG_DIE_TEMP_CONFIG 0x42
/* ID */
#define REG_REV_ID          0xFE
#define REG_DEVICE_ID         0xFF
/* Interrupt Bits */
#define INT_A_FULL          (1 << 7)  // FIFO almost full
#define INT_PPG_RDY         (1 << 6)  // New sample ready
#define INT_ALC_OVF         (1 << 5)  // Ambient light overflow
#define INT_PROX            (1 << 4)  // Proximity interrupt
/* FIFO Configuration */
#define FIFO_A_FULL_MASK    0x7F      // FIFO almost full threshold
#define FIFO_ROLLS_ON_FULL  (1 << 1)  // Roll over on full
#define FLUSH_FIFO          (1 << 4)  // Flush FIFO
/* PPG Configuration Bits */
#define PPG_ADC_RANGE_4096  (0 << 5)  // 4096 nA
#define PPG_ADC_RANGE_8192  (1 << 5)  // 8192 nA
#define PPG_ADC_RANGE_16384 (2 << 5)  // 16384 nA
#define PPG_ADC_RANGE_32768 (3 << 5)  // 32768 nA
#define PPG_SR_50HZ         (0 << 2)  // 50 samples/sec
#define PPG_SR_100HZ        (1 << 2)  // 100 samples/sec
#define PPG_SR_200HZ        (2 << 2)  // 200 samples/sec
#define PPG_SR_400HZ        (3 << 2)  // 400 samples/sec
#define PPG_LED_PW_68US     (0 << 0)  // 68.95 μs
#define PPG_LED_PW_117US    (1 << 0)  // 117.78 μs
#define PPG_LED_PW_215US    (2 << 0)  // 215.44 μs
#define PPG_LED_PW_410US    (3 << 0)  // 410.75 μs
/* System Control Bits */
#define SYSTEM_RESET        (1 << 0)
#define SHUTDOWN            (1 << 1)


/* ========================== CONSTANTS & BUFFERS =========================== */
#define MAX_FIFO_SAMPLES    112 // 128 - 16
#define SAMPLES_PER_READ    32   
#define BYTES_PER_SAMPLE    3   // 19-bit data
#define LED_CHANNELS        2   // red & ir

static uint8_t raw_buffer[SAMPLES_PER_READ * BYTES_PER_SAMPLE];
static int32_t red_samples[SAMPLES_PER_READ];
static int32_t ir_samples[SAMPLES_PER_READ];

// Get the device reference from the devicetree
static const struct i2c_dt_spec i2c_dev = I2C_DT_SPEC_GET(DT_NODELABEL(max30102));

/* =========================== HELPER FUNCTIONS ============================= */

static int max30102_write_reg(uint8_t reg, uint8_t value){
    return i2c_reg_write_byte_dt(&i2c_dev, reg, value);
}

static int max30102_read_reg(uint8_t reg, uint8_t *value){
    return i2c_reg_read_byte_dt(&i2c_dev, reg, value);
}

static int max30102_read_fifo_count(uint8_t *count){
    return max30102_read_reg(REG_FIFO_DATA_COUNT, count);
}

/* ============================ INITIALIZATION ============================== */

int max30102_init(void) {
    if (!device_is_ready(i2c_dev.bus)) {
        LOG_ERR("i2c bus not ready");
        return -ENODEV;}

    // Check if there is connection
    uint8_t device_id, rev_id; // part id, revision id
    
    // Read part id
    int ret = max30102_read_reg(REG_DEVICE_ID, &device_id);
    if (ret != 0) {
        LOG_ERR("Failed to read Part ID: %d", ret);
        return ret;
    }

    // Read revision id
    ret = max30102_read_reg(REG_REV_ID, &rev_id);
    if (ret != 0) {
        LOG_ERR("Failed to read Revision ID: %d", ret);
        return ret;
    }

    LOG_INF("MAX30102 detected. Part ID: 0x%02x, Rev ID: 0x%02x", device_id, rev_id);

    if (device_id != 0x15) LOG_WRN("Unexpected Part ID. Expected 0x15, got 0x%02x", device_id);

    // Reset the device
    ret = max30102_write_reg(REG_SYSTEM_CONTROL, SYSTEM_RESET);
    if (ret != 0) {
        LOG_ERR("Failed to reset device: %d", ret);
        return ret;
    }
    k_msleep(100); // wait for reset to finish

    // Configure FIFO
    ret = max30102_write_reg(REG_FIFO_CONFIG_1, 0x0F); // Almost full 
    if (ret != 0) {
        LOG_ERR("Failed to configure FIFO: %d", ret);
        return ret;
    }

    // FIFO rollover when full
    ret = max30102_write_reg(REG_FIFO_CONFIG_2, FIFO_ROLLS_ON_FULL);
    if (ret != 0) {
        LOG_ERR("Failed to set FIFO rollover: %d", ret);
        return ret;
    }

    // Configure PPG (Photoplethysmogram): ADC Range: 16384 nA, Sample Rate: 100 Hz, LED Pulse Width: 411 μs
    uint8_t ppg_config1 = PPG_ADC_RANGE_16384 | PPG_SR_100HZ | PPG_LED_PW_410US;
    ret = max30102_write_reg(REG_PPG_CONFIG_1, ppg_config1);
    if (ret != 0) {
        LOG_ERR("Failed to configure PPG: %d", ret);
        return ret;
    }

    // Configure LED sequence: LED1 (Red) and LED2 (IR)
    ret = max30102_write_reg(REG_LED_SEQ_1, 0x21);
    if (ret != 0) {
        LOG_ERR("Failed to configure LED sequence: %d", ret);
        return ret;
    }
    uint8_t led_current = 0x7F; // ~25 mA
    ret = max30102_write_reg(REG_LED1_PA, led_current);
    if (ret != 0) {
        LOG_ERR("Failed to set LED1 current: %d", ret);
        return ret;
    }
    ret = max30102_write_reg(REG_LED2_PA, led_current);
    if (ret != 0) {
        LOG_ERR("Failed to set LED2 current: %d", ret);
        return ret;
    }

    // Configure proximity mode
    ret = max30102_write_reg(REG_PROX_INT_THRESH, 0x14); // threshold value
    if (ret != 0) {
        LOG_ERR("Failed to set proximity threshold: %d", ret);
        return ret;
    }
    // Set pilot (lower) LED current for proximity detection
    ret = max30102_write_reg(REG_PILOT_PA, 0x3F); // ~12 mA
    if (ret != 0) {
        LOG_ERR("Failed to set pilot LED current: %d", ret);
        return ret;
    }

    // Enable Interrupts: FIFO almost full and proximity interrupts
    ret = max30102_write_reg(REG_INT_ENABLE_1, INT_A_FULL | INT_PPG_RDY);
    if (ret != 0) {
        LOG_ERR("Failed to enable interrupts 1: %d", ret);
        return ret;
    }
    ret = max30102_write_reg(REG_INT_ENABLE_2, INT_PROX);
    if (ret != 0) {
        LOG_ERR("Failed to enable interrupts 2: %d", ret);
        return ret;
    }

    // Clear interrupt flags
    uint8_t temp;
    max30102_read_reg(REG_INT_STATUS_1, &temp);
    max30102_read_reg(REG_INT_STATUS_2, &temp);
    
    LOG_INF("MAX30102 initialized successfully");
    return 0;
}

/* ============================= MODE CONTROL =============================== */

void max30102_enter_normal_mode(void) {
    // Clear proximity interrupt by reading the status register
    uint8_t temp;
    max30102_read_reg(REG_INT_STATUS_2, &temp);

    // Flush the FIFO
    max30102_write_reg(REG_FIFO_CONFIG_2, FLUSH_FIFO | FIFO_ROLLS_ON_FULL);

    // Clear FIFO registers
    max30102_write_reg(REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(REG_FIFO_RD_PTR, 0x00);
    max30102_write_reg(REG_OVF_COUNTER, 0x00);

    // Exit shutdown mode
    max30102_write_reg(REG_SYSTEM_CONTROL, 0x00);
    LOG_INF("Entered normal mode - starting data collection");
}

void max30102_enter_proximity_detection_mode(void) {
    /* TODO: Put device in low-power proximity detection mode */
    // This would use only LEDC1 with pilot LED current
    // Actual implementation depends on your power requirements
    
    LOG_INF("Entered proximity mode - waiting for skin contact");
}

bool max30102_check_proximity(void) {
    uint8_t int_status;
    
    max30102_read_reg(REG_INT_STATUS_2, &int_status);
    if (int_status & INT_PROX) {
        LOG_INF("Proximity detected - object/skin contact present");
        return true;
    }
    
    return false;
}


/* ============================ DATA COLLECTION ============================= */

int max30102_read_fifo(uint8_t *sample_count, int32_t *red_out, int32_t *ir_out) {
    // Find out how many samples are waiting
    uint8_t fifo_count;
    uint8_t samples_to_read;
    int ret = max30102_read_fifo_count(&fifo_count);
    if (ret != 0) {
        LOG_ERR("Failed to read FIFO count: %d", ret);
        return ret;
    }
    if (fifo_count == 0) {
        *sample_count = 0;
        return 0; // no data 
    }
    samples_to_read = (fifo_count > SAMPLES_PER_READ) ? SAMPLES_PER_READ : fifo_count;

    // read FIFO data over i2c
    uint8_t reg_addr = REG_FIFO_DATA;
    uint32_t bytes_to_read = samples_to_read * LED_CHANNELS * BYTES_PER_SAMPLE; 
    ret = i2c_write_read_dt(&i2c_dev, &reg_addr, 1, raw_buffer, bytes_to_read);
    if (ret != 0) {
        LOG_ERR("FIFO Read Failed: %d", ret);
        return;
    }

    // Parse the data: based on psuedo code provided in the datasheet
    // MAX30102 FIFO format: [Red_MSB][Red_MID][Red_LSB][IR_MSB][IR_MID][IR_LSB]...
    for (int i = 0; i < samples_to_read; i++) {
        // Red channel (first 3 bytes)
        int32_t red_val = ((int32_t)raw_buffer[i*6 + 0] << 16) |
                          ((int32_t)raw_buffer[i*6 + 1] << 8) |
                          ((int32_t)raw_buffer[i*6 + 2]);
        red_val &= 0x3FFFF; // 18-bit data
        red_samples[i] = red_val;
        // IR channel (next 3 bytes)
        int32_t ir_val = ((int32_t)raw_buffer[i*6 + 3] << 16) |
                         ((int32_t)raw_buffer[i*6 + 4] << 8) |
                         ((int32_t)raw_buffer[i*6 + 5]);
        ir_val &= 0x3FFFF; // 18-bit data
        ir_samples[i] = ir_val;
    }

    // Copy data to output buffers and count pointer
    if (red_out != NULL) memcpy(red_out, red_samples, samples_to_read * sizeof(int32_t));
    if (ir_out != NULL) memcpy(ir_out, ir_samples, samples_to_read * sizeof(int32_t));
    *sample_count = samples_to_read;
    LOG_DBG("Read %d samples from FIFO", samples_to_read);
    return 0;
}

/* ======================== HEART RATE CALCULATION ========================== */

#define MIN_HEART_RATE  40
#define MAX_HEART_RATE  220
#define SAMPLE_RATE     100  // Hz (from PPG config)

static int32_t last_peak_value = 0;
static uint32_t last_peak_time = 0;
static uint8_t calculated_hr = 0;

uint8_t max30102_calculate_heartrate(int32_t *ir_samples, uint8_t num_samples)
{
    static int32_t baseline = 0;
    static bool baseline_initialized = false;
    int32_t threshold;
    uint32_t current_time = k_uptime_get_32();

    if (num_samples == 0) return calculated_hr; // last known value

    // Initialize baseline on first run
    if (!baseline_initialized) {
        int64_t sum = 0;
        for (int i = 0; i < num_samples; i++) {
            sum += ir_samples[i];
        }
        baseline = sum / num_samples;
        baseline_initialized = true;
        return 0;
    }

    // Update baseline (slow moving average)
    int64_t sum = 0;
    for (int i = 0; i < num_samples; i++) {
        sum += ir_samples[i];
    }
    int32_t current_avg = sum / num_samples;
    baseline = (baseline * 95 + current_avg * 5) / 100; // Slow adaptation
    threshold = baseline + (baseline / 10); // 10% above baseline

    // Peak detection
    for (int i = 1; i < num_samples - 1; i++) {
        // Check if this is a peak (higher than neighbors and above threshold)
        if (ir_samples[i] > ir_samples[i-1] &&
            ir_samples[i] > ir_samples[i+1] &&
            ir_samples[i] > threshold) {
            // Calculate time since last peak
            if (last_peak_time > 0) {
                uint32_t time_diff = current_time - last_peak_time;
                if (time_diff > 0) {
                    // Calculate HR in BPM
                    uint32_t hr = (60000 / time_diff); // Convert ms to BPM
                    // Validate range
                    if (hr >= MIN_HEART_RATE && hr <= MAX_HEART_RATE) {
                        calculated_hr = (uint8_t)hr;
                        LOG_DBG("Heart rate: %d BPM (interval: %d ms)", 
                                calculated_hr, time_diff);
                    }
                }
            }
            
            last_peak_value = ir_samples[i];
            last_peak_time = current_time;
        }
    }
    return calculated_hr;
}

/* ============================== PUBLIC API ================================ */

uint8_t max30102_read_heartrate(void) {
    uint8_t sample_count;
    int32_t red[SAMPLES_PER_READ];
    int32_t ir[SAMPLES_PER_READ];
    int ret;

    // Read samples from FIFO */
    ret = max30102_read_fifo(&sample_count, red, ir);
    if (ret != 0 || sample_count == 0) return calculated_hr; // last known value

    // Calculate heart rate from IR channel
    return max30102_calculate_heartrate(ir, sample_count);
}