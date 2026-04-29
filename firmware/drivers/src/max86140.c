#include "max86140.h"
#include <errno.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_CS_GPIO_DEBUG)
#include <zephyr/drivers/gpio.h>
#if IS_ENABLED(CONFIG_GPIO_NRFX)
#include <zephyr/dt-bindings/gpio/nordic-nrf-gpio.h>
#endif
#if defined(CONFIG_SOC_SERIES_NRF52X)
#include <hal/nrf_gpio.h>
#endif
#endif
#include <zephyr/logging/log.h>
#include <limits.h>
#include <string.h>

LOG_MODULE_REGISTER(max86140, LOG_LEVEL_INF);

#if IS_ENABLED(CONFIG_VITABAND_BLE_HR_BODY_TEST)
#undef LOG_DBG
#undef LOG_INF
#undef LOG_WRN
#undef LOG_ERR
#define LOG_DBG(...) ((void)0)
#define LOG_INF(...) ((void)0)
#define LOG_WRN(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#endif

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
#define REG_PPG_CONFIG_2    0x12   /* PPG_SR[4:0] @ [7:3], SMP_AVE[2:0] @ [2:0] */
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
 *   [7:6] PPG1_ADC_RGE   0=4μA 1=8μA 2=16μA 3=32μA — larger FS ⇒ same photocurrent ⇒ lower codes (fights ADC rail).
 *   [5:4] PPG_TINT       0=14.8μs 1=29.4μs 2=58.7μs 3=117.3μs
 *   [1]   LP_MODE        1=low-power
 */
#define PPG_ADC_RGE_16UA    (0x2 << 6)
#define PPG_ADC_RGE_32UA    (0x3 << 6)
#define PPG_TINT_117US      (0x3 << 4)
#define PPG_LP_MODE         (1 << 1)

/* ── PPG Config 2 (0x12): PPG_SR[4:0] in bits [7:3], SMP_AVE[2:0] in [2:0] ──
 *   Datasheet Rev 5 table “PPG Configuration 2”: code is **5-bit PPG_SR**, not raw Hz.
 *   With **2 LED exposures** (IR+Red), use a row with **Pulses Per Sample N = 2**:
 *     0x07 → ~50 sps   (N=2)   — **not** 100 Hz
 *     0x09 → ~100 sps  (N=2)   — matches LED_SEQ_1 with LED1+LED2
 *     0x04 → ~200 sps  (N=1)   — IR-only LED seq; one FIFO word per tick (IR tag).
 *     0x05 → ~400 sps  (N=1)   — IR-only.
 *   Wrong code + SAMPLE_RATE_HZ mismatch → BPM scaled wrong (~2× high when using 0x07).
 */
#define PPG_SR_100SPS       ((0x09U << 3) & 0xF8U)
#define PPG_SR_200SPS_N1    ((0x04U << 3) & 0xF8U)
#define PPG_SR_400SPS_N1    ((0x05U << 3) & 0xF8U)
#define PPG_SMP_AVE_1       (0x00) /* SMP_AVE[2:0]=000 → no on-chip averaging */

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
 * SPI FRAMING (datasheet “Single-Word SPI…” + MAX86141_Arduino reference)
 *
 * Every word is 3 bytes / 24 SCLK rising edges while CSB is low:
 *   Byte 0: register address A[7:0] (no R/W stuffed into this byte).
 *   Byte 1: command — 0xFF = read, 0x00 = write (READ_EN / WRITE_EN).
 *   Byte 2: data byte read from or written to that register.
 *
 * Clock: SPI mode 3 (Maxim reference drivers; Mode 0 also matches Fig.34 idle-low —
 *   use mode 3 if FIFO reads 0xFF… or saturated garbage.)
 *
 * FIFO burst (datasheet FIFO section): first **16** clocks = byte0 address +
 * byte1 read command (same as starting a normal read at FIFO_DATA), then
 * **24 clocks per FIFO word** (3 bytes/sample) in one CS assertion.
 * ══════════════════════════════════════════════════════════════════════════ */
#define MAX86140_CMD_WRITE  0x00U
#define MAX86140_CMD_READ   0xFFU

/* ══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ══════════════════════════════════════════════════════════════════════════ */
#define SAMPLES_PER_READ    32
#define BYTES_PER_SAMPLE    3      /* 24-bit FIFO word over 3 bytes */
/*
 * IR+Red N=2: two FIFO words per IR sample (IR then red).
 * IR-only N=1: one word per IR sample — allow a larger burst so ~200/400 sps does not
 * truncate FIFO_DATA_COUNT when the host polls every ~220 ms.
 */
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_100_IRRED)
#define MAX_FIFO_WORDS      (SAMPLES_PER_READ * 2)
#define FIFO_IR_WORD_STRIDE 2
#else
#define MAX_FIFO_WORDS      (SAMPLES_PER_READ * 4)
#define FIFO_IR_WORD_STRIDE 1
#endif
#define MIN_HEART_RATE      35
#define MAX_HEART_RATE      220
/*
 * Min time between HR-producing peaks. Arm motion adds extra bumps — slightly wider
 * than 250 ms rejects duplicate peaks within one beat (~214 BPM ceiling).
 * Wrist profile stretches this a bit for noisier optical coupling.
 */
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_WRIST_PROFILE)
#define HR_MIN_PEAK_INTERVAL_MS  452U
#else
#define HR_MIN_PEAK_INTERVAL_MS  280U
#endif

/* IR ADC ~19b max; batch pinned to rail ⇒ intervals meaningless — flush median buffer */
#define IR_ADC_NEAR_FULL           520000

/*
 * Prominence when ≥4 peaks; tuned so duplicate humps don’t read high vs reference.
 * Post-exercise / motion can yield ≥5 local maxima per batch; **do not** blanket-skip
 * on peak count alone — that froze HR low (logs: peaks=6 skip=1 iv_raw=0 while ref ~96).
 * Ambiguity is handled by prominence vs second-best peak.
 */
#define HR_PEAK_PROM_PEAKS_GE    4U
#define HR_PEAK_PROM_MIN_ABS    52
#define HR_PEAK_PROM_PP_NUM     10

/* Median of last N accepted beat intervals (ms) — damps ~500ms ghost beats vs ~640ms real. */
#define HR_IV_MEDIAN_CAP         5U
#define HR_IV_MEDIAN_MIN_N       3U
/* Raw gaps > ~1100ms are usually “missed beat” stacks after skips — poison median (logs: iv_smooth=1120). */
#define HR_IV_HIST_PUSH_MAX_MS   1100U

#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_WRIST_PROFILE)
#define HR_PEAK_MARGIN_DIV  4   /* tighter than /5 — fewer motion ripples as peaks */
#define HR_CREST_PP_NUM     50  /* upper half of swing must qualify as systolic */
#else
#define HR_PEAK_MARGIN_DIV  5
#define HR_CREST_PP_NUM     45
#endif

/*
 * Finger off: empty FIFO streak. main_max86140_hr polls at SAMPLE_PERIOD_MS (~220 ms);
 * 20 loops ≈ 4.4 s without FIFO data before HR state clears.
 */
#define HR_EMPTY_FIFO_LOOPS   20U

/* IR ADC rail (~19-bit max). Flatline at rail ⇒ no PP ⇒ HR=0 — dim LEDs. */
#define IR_SAT_FLAT_MIN_PP    4000    /* below this PP while near rail = useless for peaks */
#define IR_SAT_NEAR_FULL      480000  /* react before hard-clamp at 524287 */
#define LED_PA_STEP           8
#define LED_PA_FLOOR          0x10
#define LED_PA_INIT           0x48    /* prior “kinda working” bring-up level; autogain still trims rail-flat */
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_WRIST_PROFILE)
#define LED_PA_START          0x58U   /* high starts (0x72) + autogain → rail-flat pp=0; tune up if needed */
#else
#define LED_PA_START          LED_PA_INIT
#endif
/*
 * Nominal IR sample rate — must match Kconfig PPG mode + init() PPG_SR & LED seq.
 */
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_400_IR)
#define SAMPLE_RATE_HZ  400
#elif IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_200_IR)
#define SAMPLE_RATE_HZ  200
#else
#define SAMPLE_RATE_HZ  100
#endif
/*
 * FIFO word — Rev.5 Table 6 + datasheet FIFO read pseudo-code:
 *   TAG[4:0] = FIFO_DATA[23:19];  ADC = FIFO_DATA[18:0] (mask lower 19 bits).
 *   Equivalently: tag = (MSB_byte >> 3) & 0x1F; adc = raw24 & 0x7FFFF.
 * Table 3: PPG1 LEDC1 (IR) = tag 0x01, LEDC2 (Red) = 0x02.
 */
#define FIFO_TAG_IR         0x01U

static inline uint32_t max86140_fifo_tag(uint32_t raw24)
{
    return (raw24 >> 19) & 0x1FU;
}

static inline uint32_t max86140_fifo_adc_u19(uint32_t raw24)
{
    return raw24 & 0x7FFFFU;
}

/* ══════════════════════════════════════════════════════════════════════════
 * SPI DEVICE — from devicetree
 * ══════════════════════════════════════════════════════════════════════════ */
static const struct spi_dt_spec spi_dev =
    SPI_DT_SPEC_GET(DT_NODELABEL(max86140),
                    SPI_OP_MODE_MASTER |
                    SPI_WORD_SET(8)    |
                    SPI_TRANSFER_MSB |
                    SPI_MODE_CPOL |
                    SPI_MODE_CPHA,
                    0);

#if IS_ENABLED(CONFIG_VITABAND_MAX86140_CS_GPIO_DEBUG)
/*
 * CS from spi { cs-gpios } — SPI_CS_GPIOS_DT_SPEC_GET(max86140).
 *
 * Nordic: OR in NRF_GPIO_DRIVE_* (recommended H0H1); bare GPIO_OUTPUT can fall
 * through S0S1 depending on toolchain but explicit drive is safest.
 *
 * gpio_pin_set(port,pin,val) sets **physical** output level (not gpio_pin_set_dt).
 *
 * nrf_gpio PORT->OUT readback proves the SoC latched LOW/HIGH without probing P0.xx.
 */
#define CS_DBG_HOLD_MS 10000U

static unsigned int cs_dbg_mcu_out_level(const struct gpio_dt_spec *cs)
{
#if defined(CONFIG_SOC_SERIES_NRF52X)
	const struct device *p0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

	if (cs->port == p0) {
		return (unsigned int)((nrf_gpio_port_out_read(NRF_P0) >> cs->pin) & 1U);
	}
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio1), okay)
	const struct device *p1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));

	if (cs->port == p1) {
		return (unsigned int)((nrf_gpio_port_out_read(NRF_P1) >> cs->pin) & 1U);
	}
#endif
#endif
	return 2U;
}

static void max86140_cs_scope_probe(void)
{
	// const struct gpio_dt_spec cs =
	// 	SPI_CS_GPIOS_DT_SPEC_GET(DT_NODELABEL(max86140));

	// printk("max86140 cs-debug: MCU_OUT reads nRF PORT OUT reg (no probe needed).\n");
	// printk("max86140 cs-debug: MCU_OUT ok but CSB stuck high → past MCU (shifter/wrong pad).\n");

	// if (!gpio_is_ready_dt(&cs)) {
	// 	printk("max86140 cs-debug: no gpio CS in DT (hw CS only?)\n");
	// 	return;
	// }

	gpio_flags_t flags = GPIO_OUTPUT_HIGH;
#if IS_ENABLED(CONFIG_GPIO_NRFX)
	flags |= NRF_GPIO_DRIVE_H0H1;
#endif

	/* Bypass DT phandle — directly grab P0.14 */
    const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(gpio0)) {
        LOG_ERR("gpio0 not ready (CS GPIO debug)");
        return -ENODEV;
    }

    while (1) {
        gpio_pin_configure(gpio0, 14, flags);
        k_msleep(3000);
        gpio_pin_set(gpio0, 14, 0);
        k_msleep(3000);
    }
}
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * STATIC BUFFERS
 * ══════════════════════════════════════════════════════════════════════════ */
/* Worst-case burst: 2-byte header (REG + READ cmd) + FIFO payload */
#define TX_BUF_LEN   (2 + MAX_FIFO_WORDS * BYTES_PER_SAMPLE)
#define RX_BUF_LEN   (2 + MAX_FIFO_WORDS * BYTES_PER_SAMPLE)

static uint8_t tx_buf[TX_BUF_LEN];
static uint8_t rx_buf[RX_BUF_LEN];

static int32_t ir_samples[SAMPLES_PER_READ];

/* Heart rate state */
static uint8_t  calculated_hr   = 0;
static uint8_t  hr_output_ema   = 0; /* smoothed BPM; cleared on signal loss */
/* Monotonic IR-sample index — peak times = (tick * dt_ms), valid across FIFO reads */
static uint32_t ir_sample_index = 0;
static uint32_t last_peak_tick  = 0;
static int32_t  hr_baseline_val = 0;
static bool     hr_baseline_ready;

/* Mirrors REG_LED{1,2}_PA — auto-dim on IR rail-flat saturation */
static uint8_t  led_pa_ir   = LED_PA_START;
static uint8_t  led_pa_red  = LED_PA_START;

static uint32_t hr_iv_hist[HR_IV_MEDIAN_CAP];
static uint8_t  hr_iv_hist_n;

static void hr_iv_hist_clear(void)
{
    hr_iv_hist_n = 0U;
}

static void hr_iv_hist_push(uint32_t iv_ms)
{
    if (hr_iv_hist_n < HR_IV_MEDIAN_CAP) {
        hr_iv_hist[hr_iv_hist_n++] = iv_ms;
    } else {
        memmove(&hr_iv_hist[0], &hr_iv_hist[1],
                (size_t)(HR_IV_MEDIAN_CAP - 1U) * sizeof(hr_iv_hist[0]));
        hr_iv_hist[HR_IV_MEDIAN_CAP - 1U] = iv_ms;
    }
}

static uint32_t median_u32_intervals(const uint32_t *src, uint8_t n)
{
    uint32_t s[HR_IV_MEDIAN_CAP];
    uint8_t  i, j;

    if (n == 0U || n > HR_IV_MEDIAN_CAP) {
        return 0U;
    }

    for (i = 0U; i < n; i++) {
        s[i] = src[i];
    }

    for (i = 1U; i < n; i++) {
        uint32_t key = s[i];

        j = i;
        while (j > 0U && s[j - 1U] > key) {
            s[j] = s[j - 1U];
            j--;
        }
        s[j] = key;
    }

    return s[n / 2U];
}

static void hr_contact_lost_reset(void)
{
    calculated_hr = 0;
    hr_output_ema = 0;
    last_peak_tick = 0;
    hr_baseline_ready = false;
    hr_baseline_val = 0;
    hr_iv_hist_clear();
}

/* LED PA step-down only — keep baseline + EMA; reset peak anchor so intervals stay valid after gain change */
static void hr_reanchor_after_led_autogain(void)
{
    last_peak_tick = 0U;
    hr_iv_hist_clear();
}

static uint8_t  dbg_fifo_reg_count;
static uint8_t  dbg_fifo_words;
static uint8_t  dbg_fifo_n;
static int32_t  dbg_ir_min;
static int32_t  dbg_ir_max;
static int32_t  dbg_mean;
static int32_t  dbg_thresh;
static uint8_t  dbg_peak_count;
static uint32_t dbg_interval_ms;
static uint32_t dbg_cand_bpm;
static uint8_t  dbg_rejected;
static uint8_t  dbg_baseline_ready;
static int32_t  dbg_second_best_amp;
static int32_t  dbg_prom_delta;
static uint8_t  dbg_hr_skip_code; /* 0=none, 2=prominence fail (≥4 peaks) */
static uint32_t dbg_iv_smooth_ms;

/* Hook kept for call sites; verbose FIFO/HR tracing removed from console. */
static void max86140_rtt_status(uint32_t t, int fifo_ret, bool have_ir_batch)
{
	ARG_UNUSED(t);
	ARG_UNUSED(fifo_ret);
	ARG_UNUSED(have_ir_batch);
}

/* ══════════════════════════════════════════════════════════════════════════
 * LOW-LEVEL SPI HELPERS
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write a single byte to a register.
 */
static int max86140_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[3] = { reg, MAX86140_CMD_WRITE, val };

    struct spi_buf tx_spi = { .buf = tx, .len = 3 };
    struct spi_buf_set tx_set = { .buffers = &tx_spi, .count = 1 };

    int ret = spi_write_dt(&spi_dev, &tx_set);

    return ret;
}

/**
 * @brief Read a single byte from a register.
 */
static int max86140_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t tx[3] = { reg, MAX86140_CMD_READ, 0x00 };
    uint8_t rx[3] = { 0 };

    struct spi_buf tx_spi = { .buf = tx, .len = 3 };
    struct spi_buf rx_spi = { .buf = rx, .len = 3 };
    struct spi_buf_set tx_set = { .buffers = &tx_spi, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_spi, .count = 1 };

    int ret = spi_transceive_dt(&spi_dev, &tx_set, &rx_set);

    if (ret == 0) {
        *val = rx[2];
    }
    return ret;
}

/**
 * @brief Burst-read N 3-byte FIFO words from REG_FIFO_DATA.
 *
 * After the 16-clock header (address byte + READ cmd), each FIFO word is 24 clocks.
 */
static int max86140_read_fifo_burst(uint8_t n_words, uint8_t *data_out)
{
    uint32_t total = 2 + (uint32_t)n_words * BYTES_PER_SAMPLE;

    memset(tx_buf, 0, total);
    tx_buf[0] = REG_FIFO_DATA;
    tx_buf[1] = MAX86140_CMD_READ;

    struct spi_buf tx_spi = { .buf = tx_buf, .len = total };
    struct spi_buf rx_spi = { .buf = rx_buf, .len = total };
    struct spi_buf_set tx_set = { .buffers = &tx_spi, .count = 1 };
    struct spi_buf_set rx_set = { .buffers = &rx_spi, .count = 1 };

    int ret = spi_transceive_dt(&spi_dev, &tx_set, &rx_set);
    if (ret == 0) {
        memcpy(data_out, &rx_buf[2], n_words * BYTES_PER_SAMPLE);
    }
    return ret;
}

/* ══════════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ══════════════════════════════════════════════════════════════════════════ */

int max86140_init(void)
{
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_CS_GPIO_DEBUG)
	max86140_cs_scope_probe();
#endif

    if (!spi_is_ready_dt(&spi_dev)) {
        LOG_ERR("SPI bus not ready for MAX86140");
        return -ENODEV;
    }
    /* ── 1. Software reset, then Part ID — read after reset avoids junk at cold start ── */
    int ret = max86140_write_reg(REG_SYSTEM_CTRL, SYS_RESET);
    if (ret != 0) {
        LOG_ERR("Reset failed: %d", ret);
        return ret;
    }
    k_msleep(10);   /* datasheet: allow reset / POR to complete */

    uint8_t part_id = 0;
    while (part_id == 0 || part_id == 0xFF) {
        ret = max86140_read_reg(REG_PART_ID, &part_id);
        if (ret != 0) {
            LOG_ERR("Failed to read Part ID: %d", ret);
            return ret;
        }
        if (part_id != MAX86140_PART_ID) {
            LOG_ERR("Unexpected Part ID: 0x%02x (expected 0x%02x) — retry",
                    part_id, MAX86140_PART_ID);
            k_msleep(100);
        }
    }
    ret = max86140_read_reg(REG_PART_ID, &part_id);
    if (ret != 0) {
        LOG_ERR("Failed to read Part ID: %d", ret);
        return ret;
    }

    uint8_t rev_id = 0;
    max86140_read_reg(REG_REV_ID, &rev_id);

    if (part_id != MAX86140_PART_ID) {
        LOG_ERR("Unexpected Part ID: 0x%02x rev 0x%02x (expected 0x%02x); "
                "hint 0x00 = MISO/SPI mode/shift/overlay",
                part_id, rev_id, MAX86140_PART_ID);
        return -EIO;
    }
    LOG_DBG("MAX86140 detected. Part ID: 0x%02x", part_id);

    /* Clear LED range scaling (31 mA full scale per channel); stale non-zero skews PA. */
    ret = max86140_write_reg(REG_LED_RANGE_1, 0x00);
    if (ret != 0) {
        LOG_ERR("LED_RANGE_1 failed: %d", ret);
        return ret;
    }

    /* ── 3. Configure FIFO ──
     *   FIFO_CONFIG_1: A_FULL threshold = 0x10 (interrupt at 112 samples)
     *   FIFO_CONFIG_2: FIFO_ROLL_ON_FULL enabled, stat cleared on read
     */
    ret = max86140_write_reg(REG_FIFO_CONFIG_1, 0x10);
    if (ret != 0) {
        LOG_ERR("FIFO config 1 failed: %d", ret);
        return ret;
    }

    ret = max86140_write_reg(REG_FIFO_CONFIG_2,
                             FIFO_ROLL_ON_FULL | FIFO_STAT_CLR);
    if (ret != 0) {
        LOG_ERR("FIFO config 2 failed: %d", ret);
        return ret;
    }

    /* ── 4. Configure PPG ──
     *   ADC range: 16μA, integration time: 117.3μs, LP_MODE on
     *   Sample rate: 100 sps, LED settling: 6μs
     */
    /* LP_MODE off for bring-up — re-enable later for power saving (≤128 sps only). */
    ret = max86140_write_reg(REG_PPG_CONFIG_1,
                             PPG_ADC_RGE_32UA | PPG_TINT_117US);
    if (ret != 0) {
        LOG_ERR("PPG config 1 failed: %d", ret);
        return ret;
    }

#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_400_IR)
    ret = max86140_write_reg(REG_PPG_CONFIG_2,
                             PPG_SR_400SPS_N1 | PPG_SMP_AVE_1);
#elif IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_200_IR)
    ret = max86140_write_reg(REG_PPG_CONFIG_2,
                             PPG_SR_200SPS_N1 | PPG_SMP_AVE_1);
#else
    ret = max86140_write_reg(REG_PPG_CONFIG_2,
                             PPG_SR_100SPS | PPG_SMP_AVE_1);
#endif
    if (ret != 0) {
        LOG_ERR("PPG config 2 failed: %d", ret);
        return ret;
    }
    /* ── 5. Configure LED sequence ──
     *   Slot 1: LED1 (IR)
     *   Slot 2: LED2 (Red)  — useful if you want SpO2 later
     *   Slots 3-6: disabled
     *
     *   REG_LED_SEQ_1 = [LEDC2 | LEDC1] = [LED2 << 4 | LED1]
     *   IR-only fast modes (N=1): only LED1 in slot 1 (red off) per datasheet N=1 path.
     */
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_100_IRRED)
    ret = max86140_write_reg(REG_LED_SEQ_1,
                             (LEDC_LED2 << 4) | LEDC_LED1);
#else
    ret = max86140_write_reg(REG_LED_SEQ_1, LEDC_LED1);
#endif
    if (ret != 0) {
        LOG_ERR("LED seq 1 failed: %d", ret);
        return ret;
    }

    ret = max86140_write_reg(REG_LED_SEQ_2, 0x00);  /* slots 3+4 off */
    if (ret != 0) {
        LOG_ERR("LED seq 2 failed: %d", ret);
        return ret;
    }

    ret = max86140_write_reg(REG_LED_SEQ_3, 0x00);  /* slots 5+6 off */
    if (ret != 0) {
        LOG_ERR("LED seq 3 failed: %d", ret);
        return ret;
    }

    /* ── 6. Set LED pulse amplitudes (see LED_PA_* — high PA ⇒ IR rail / ac=0 / HR stuck) ── */
    led_pa_ir = LED_PA_START;
    led_pa_red = LED_PA_START;

    ret = max86140_write_reg(REG_LED1_PA, led_pa_ir);
    if (ret != 0) {
        LOG_ERR("LED1 PA failed: %d", ret);
        return ret;
    }

#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_100_IRRED)
    ret = max86140_write_reg(REG_LED2_PA, led_pa_red);
    if (ret != 0) {
        LOG_ERR("LED2 PA failed: %d", ret);
        return ret;
    }
#else
    ret = max86140_write_reg(REG_LED2_PA, 0x00);
    if (ret != 0) {
        LOG_ERR("LED2 PA off failed: %d", ret);
        return ret;
    }

    led_pa_red = 0U;
#endif

    /* ── 7. Enable interrupts: A_FULL and PPG_RDY ── */
    ret = max86140_write_reg(REG_INT_ENABLE_1, INT_A_FULL | INT_PPG_RDY);
    if (ret != 0) {
        LOG_ERR("INT enable failed: %d", ret);
        return ret;
    }

    /* ── 8. Clear any pending interrupt flags ── */
    uint8_t dummy;
    max86140_read_reg(REG_INT_STATUS_1, &dummy);
    max86140_read_reg(REG_INT_STATUS_2, &dummy);

    /* ── 9. Flush FIFO before starting ── */
    ret = max86140_write_reg(REG_FIFO_CONFIG_2,
                             FIFO_FLUSH | FIFO_ROLL_ON_FULL | FIFO_STAT_CLR);
    if (ret != 0) {
        LOG_ERR("FIFO flush failed: %d", ret);
        return ret;
    }

    /* Re-arm FIFO config without flush bit */
    ret = max86140_write_reg(REG_FIFO_CONFIG_2,
                             FIFO_ROLL_ON_FULL | FIFO_STAT_CLR);
    if (ret != 0) {
        LOG_ERR("FIFO re-arm failed: %d", ret);
        return ret;
    }

    /* ── 10. Exit shutdown ── */
    ret = max86140_write_reg(REG_SYSTEM_CTRL, 0x00);
    if (ret != 0) {
        LOG_ERR("Exit shutdown failed: %d", ret);
        return ret;
    }

    LOG_DBG("MAX86140 initialized successfully");

    /* Fresh HR state matches a freshly configured part (avoids stale EMA across experiments). */
    hr_contact_lost_reset();

    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * FIFO READ
 * ══════════════════════════════════════════════════════════════════════════ */

static int max86140_read_fifo(uint8_t *sample_count_out, int32_t *ir_out)
{
    uint8_t fifo_count = 0;
    int ret = max86140_read_reg(REG_FIFO_DATA_COUNT, &fifo_count);
    /* #region agent log */
    dbg_fifo_reg_count = fifo_count;
    /* #endregion */
    if (ret != 0) {
        LOG_ERR("FIFO count read failed: %d", ret);
        return ret;
    }
    if (fifo_count == 0) {
        dbg_fifo_words = 0;
        dbg_fifo_n = 0;
        *sample_count_out = 0;
        return 0;
    }

    /*
     * REG_FIFO_DATA_COUNT = number of 3-byte FIFO words (datasheet).
     * IR+Red N=2: sequence [IR][Red][IR][Red]… — align to first IR tag, stride 2.
     * IR-only N=1: each word is IR — stride 1 after first IR tag.
     */
    uint8_t words = fifo_count;

    if (words > MAX_FIFO_WORDS) {
        words = MAX_FIFO_WORDS;
    }

    if (words == 0U) {
        dbg_fifo_words = 0;
        dbg_fifo_n = 0;
        *sample_count_out = 0;
        return 0;
    }

    /* #region agent log */
    dbg_fifo_words = words;
    /* #endregion */

    static uint8_t raw[MAX_FIFO_WORDS * BYTES_PER_SAMPLE];
    ret = max86140_read_fifo_burst(words, raw);
    if (ret != 0) {
        LOG_ERR("FIFO burst read failed: %d (len=%u words)", ret,
                (unsigned int)words);
        return ret;
    }

    int ir_start = -1;

    for (unsigned w = 0; w < (unsigned int)words; w++) {
        int base = (int)(w * 3U);

        if (raw[(size_t)base + 0U] == 0xFF && raw[(size_t)base + 1U] == 0xFF &&
            raw[(size_t)base + 2U] == 0xFF) {
            continue;
        }

        uint32_t raw32 = ((uint32_t)raw[(size_t)base + 0U] << 16) |
                         ((uint32_t)raw[(size_t)base + 1U] << 8) |
                         (uint32_t)raw[(size_t)base + 2U];

        if (max86140_fifo_tag(raw32) == FIFO_TAG_IR) {
            ir_start = (int)w;
            break;
        }
    }

    if (ir_start < 0) {
        *sample_count_out = 0;
        /* #region agent log */
        dbg_fifo_n = 0;
        /* #endregion */
        return 0;
    }

    uint8_t n_ir = 0;

    for (int w = ir_start; w < (int)words && n_ir < SAMPLES_PER_READ;
         w += FIFO_IR_WORD_STRIDE) {
        int base = w * 3;

        if (raw[(size_t)base + 0U] == 0xFF && raw[(size_t)base + 1U] == 0xFF &&
            raw[(size_t)base + 2U] == 0xFF) {
            continue;
        }

        uint32_t raw32 = ((uint32_t)raw[(size_t)base + 0U] << 16) |
                         ((uint32_t)raw[(size_t)base + 1U] << 8) |
                         (uint32_t)raw[(size_t)base + 2U];

        if (max86140_fifo_tag(raw32) != FIFO_TAG_IR) {
            continue;
        }

        ir_out[n_ir++] = (int32_t)max86140_fifo_adc_u19(raw32);
    }

    *sample_count_out = n_ir;
    /* #region agent log */
    dbg_fifo_n = n_ir;
    /* #endregion */
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════════
 * HEART RATE CALCULATION
 *
 * Simple peak-detection on the IR channel. Identical algorithm to the
 * MAX30102 driver but adapted for MAX86140's 19-bit ADC range.
 * ══════════════════════════════════════════════════════════════════════════ */

static uint8_t max86140_calculate_hr(int32_t *samples, uint8_t n, uint32_t batch_start_tick)
{
    /* #region agent log */
    dbg_peak_count = 0;
    dbg_interval_ms = 0;
    dbg_cand_bpm = 0;
    dbg_rejected = 0;
    dbg_hr_skip_code = 0;
    dbg_second_best_amp = INT32_MIN;
    dbg_iv_smooth_ms = 0U;
    dbg_baseline_ready = hr_baseline_ready ? 1U : 0U;
    if (n > 0) {
        dbg_ir_min = samples[0];
        dbg_ir_max = samples[0];
        for (int i = 1; i < n; i++) {
            if (samples[i] < dbg_ir_min) {
                dbg_ir_min = samples[i];
            }
            if (samples[i] > dbg_ir_max) {
                dbg_ir_max = samples[i];
            }
        }
    }
    /* #endregion */

    if (n > 0 && dbg_ir_min >= IR_ADC_NEAR_FULL &&
        dbg_ir_max >= IR_ADC_NEAR_FULL) {
        hr_iv_hist_clear();
    }

    if (n == 0) return calculated_hr;

    /* Need ≥3 IR samples for a local maximum (indices 1..n-2). */
    if (n < 3U) return calculated_hr;

    /* Compute mean of this batch */
    int64_t sum = 0;
    for (int i = 0; i < n; i++) sum += samples[i];
    int32_t batch_mean = (int32_t)(sum / n);

    if (!hr_baseline_ready) {
        hr_baseline_val = batch_mean;
        hr_baseline_ready = true;
        /* #region agent log */
        dbg_mean = batch_mean;
        dbg_thresh = 0;
        /* #endregion */
        return 0;
    }

    /* Slow-tracking baseline (95% old + 5% new) — display / debug only */
    hr_baseline_val = (hr_baseline_val * 95 + batch_mean * 5) / 100;

    /*
     * Peak gate tied to THIS batch amplitude (fixes thr > max when baseline delta was huge).
     *
     * Wrist/arm often shows *more* local maxima than the finger (motion / strap / tissue):
     * a loose margin lets ripples pass as peaks. Use one firm relative margin and a crest
     * floor so only samples in the upper part of the batch swing count as systolic peaks.
     */
    int32_t ac_pp = dbg_ir_max - dbg_ir_min;

    if (ac_pp < 1) {
        ac_pp = 1;
    }

    int32_t margin = ac_pp / HR_PEAK_MARGIN_DIV;

    if (margin < 34) {
        margin = 34;
    }

    int32_t threshold = batch_mean + margin;

    /* Upper part of this batch’s IR swing — rejects mid-range noise bumps on the arm */
    int32_t crest_min = dbg_ir_min;

    if (ac_pp >= 256) {
        crest_min = dbg_ir_min + (ac_pp * HR_CREST_PP_NUM) / 100;
    }

    /* #region agent log */
    dbg_mean = batch_mean;
    dbg_thresh = threshold;
    /* #endregion */

    const uint32_t dt_ms = 1000U / SAMPLE_RATE_HZ;

    /*
     * Count every local maximum for PP trace, but drive HR timing from **one** peak per
     * batch — the largest systolic candidate. Otherwise 3–5 ripples per ~220 ms window
     * yield interval_ms ~300–400 ms → ~150–200 BPM even when the true beat is ~1 s.
     */
    int           best_i   = -1;
    int32_t       best_amp = INT32_MIN;

    for (int i = 1; i < n - 1; i++) {
        bool is_peak = (samples[i] > samples[i - 1]) &&
                       (samples[i] > samples[i + 1]) &&
                       (samples[i] > threshold) &&
                       (samples[i] >= crest_min);

        if (!is_peak) {
            continue;
        }

        /* #region agent log */
        dbg_peak_count++;
        /* #endregion */

        if (samples[i] > best_amp) {
            best_amp = samples[i];
            best_i = i;
        }
    }

    if (best_i >= 1 && dbg_peak_count >= 2) {
        for (int i = 1; i < n - 1; i++) {
            if (i == best_i) {
                continue;
            }

            bool is_pk = (samples[i] > samples[i - 1]) &&
                         (samples[i] > samples[i + 1]) &&
                         (samples[i] > threshold) &&
                         (samples[i] >= crest_min);

            if (!is_pk) {
                continue;
            }

            if (samples[i] > dbg_second_best_amp) {
                dbg_second_best_amp = samples[i];
            }
        }
    }

    if (best_i >= 1 &&
        dbg_peak_count >= HR_PEAK_PROM_PEAKS_GE &&
        dbg_second_best_amp != INT32_MIN) {
        int32_t prom_need =
            (ac_pp * HR_PEAK_PROM_PP_NUM) / 100;

        if (prom_need < HR_PEAK_PROM_MIN_ABS) {
            prom_need = HR_PEAK_PROM_MIN_ABS;
        }

        if ((samples[best_i] - dbg_second_best_amp) < prom_need) {
            dbg_hr_skip_code = 2;
        }
    }

    if (best_i >= 1 && dbg_hr_skip_code == 0) {
        uint32_t peak_tick = batch_start_tick + (uint32_t)best_i;

        if (last_peak_tick == 0U) {
            last_peak_tick = peak_tick;
        } else {
            uint32_t dtick = (peak_tick > last_peak_tick)
                             ? (peak_tick - last_peak_tick)
                             : (last_peak_tick - peak_tick);
            uint32_t interval_ms =
                (uint32_t)((uint64_t)dtick * (uint64_t)dt_ms);

            if (interval_ms < HR_MIN_PEAK_INTERVAL_MS) {
                /* #region agent log */
                dbg_rejected = 1;
                dbg_interval_ms = interval_ms;
                dbg_cand_bpm =
                    interval_ms > 0U ? (60000U / interval_ms) : 0U;
                /* #endregion */
            } else {
                /*
                 * Logs showed median poisoned after ADC rail recovery (524287…).
                 * Only extend history when raw spacing is physiologically plausible.
                 */
                uint32_t raw_ibpm =
                    interval_ms > 0U ? (60000U / interval_ms) : 0U;

                if (raw_ibpm >= 48U && raw_ibpm <= 145U &&
                    interval_ms <= HR_IV_HIST_PUSH_MAX_MS) {
                    hr_iv_hist_push(interval_ms);
                }

                uint32_t iv_smooth = interval_ms;

                if (hr_iv_hist_n >= HR_IV_MEDIAN_MIN_N) {
                    iv_smooth =
                        median_u32_intervals(hr_iv_hist, hr_iv_hist_n);
                }

                uint32_t bpm = iv_smooth > 0U ? (60000U / iv_smooth) : 0U;

                /* #region agent log */
                dbg_interval_ms = interval_ms;
                dbg_iv_smooth_ms = iv_smooth;
                dbg_cand_bpm = bpm;
                /* #endregion */

                if (bpm >= MIN_HEART_RATE && bpm <= MAX_HEART_RATE) {
                    if (hr_output_ema == 0U) {
                        hr_output_ema = (uint8_t)bpm;
                    } else {
                        hr_output_ema = (uint8_t)(((uint32_t)hr_output_ema * 15U +
                                       bpm + 8U) /
                                      16U);
                    }

                    calculated_hr = hr_output_ema;
                    last_peak_tick = peak_tick;
                } else {
                    /* #region agent log */
                    dbg_rejected = 1;
                    /* #endregion */
                    if (bpm < MIN_HEART_RATE) {
                        last_peak_tick = peak_tick;
                    }
                }
            }
        }
    } else if (best_i >= 1 && dbg_hr_skip_code != 0) {
        dbg_rejected = 1;
    }

    dbg_prom_delta = -1;
    if (best_i >= 1 && dbg_second_best_amp != INT32_MIN) {
        dbg_prom_delta = samples[best_i] - dbg_second_best_amp;
    }

    return calculated_hr;
}

/* Clip LED drive when IR rides the ADC rail with no pulsatile variation (logs: ac=0, IR≈524287). */
static void max86140_ir_autogain_rail_flat(const int32_t *ir, uint8_t n)
{
    if (n < 8U) {
        return;
    }

    int32_t mn = ir[0];
    int32_t mx = ir[0];

    for (int i = 1; i < n; i++) {
        if (ir[i] < mn) {
            mn = ir[i];
        }
        if (ir[i] > mx) {
            mx = ir[i];
        }
    }

    int32_t pp = mx - mn;

    if (mx < IR_SAT_NEAR_FULL || pp > IR_SAT_FLAT_MIN_PP) {
        return;
    }

    if (led_pa_ir <= LED_PA_FLOOR) {
        return;
    }

    static uint32_t last_dim_ms;
    uint32_t now = k_uptime_get_32();
    /*
     * Hard-clipped FIFO (logs: min=max=524287, pp=0) needs faster settle than 2 s.
     */
    bool hard_clip = (mn >= IR_ADC_NEAR_FULL) || (mx >= IR_ADC_NEAR_FULL);
    uint32_t dim_cooldown_ms = 2000U;

    if (pp < 200 && mx >= 510000) {
        dim_cooldown_ms = 600U;
    } else if (hard_clip && pp < 800) {
        dim_cooldown_ms = 1200U;
    }

    if ((now - last_dim_ms) < dim_cooldown_ms) {
        return;
    }

    uint8_t new_pa = (uint8_t)(led_pa_ir - LED_PA_STEP);

    if (new_pa < LED_PA_FLOOR) {
        new_pa = LED_PA_FLOOR;
    }

    int wr1 = max86140_write_reg(REG_LED1_PA, new_pa);
    int wr2 = 0;

#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_100_IRRED)
    wr2 = max86140_write_reg(REG_LED2_PA, new_pa);
#endif

    if (wr1 != 0 || wr2 != 0) {
        LOG_ERR("LED PA dim SPI failed wr1=%d wr2=%d (tried 0x%02x)", wr1, wr2,
                new_pa);
        return;
    }

    last_dim_ms = now;
    led_pa_ir = new_pa;
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_100_IRRED)
    led_pa_red = new_pa;
#endif

    hr_reanchor_after_led_autogain();
}

/*
 * After aggressive dimming, PA can sit at/near floor while the waveform stays
 * rail-flat high (logs: pp≈0, codes ~517k) — no peaks ⇒ HR=0. Step back up toward
 * LED_PA_START only when clearly near floor.
 */
#define WEAK_PP_RECOVER_STREAK 25U
#define WEAK_PP_RAIL_CODE_LO    500000

static uint16_t weak_pp_recover_streak;

static void max86140_ir_recover_near_floor(const int32_t *ir, uint8_t n)
{
    if (n < 8U) {
        return;
    }

    if (led_pa_ir > (uint8_t)(LED_PA_FLOOR + 24U) ||
        led_pa_ir >= LED_PA_START) {
        weak_pp_recover_streak = 0U;
        return;
    }

    int32_t mn = ir[0];
    int32_t mx = ir[0];

    for (int i = 1; i < n; i++) {
        if (ir[i] < mn) {
            mn = ir[i];
        }
        if (ir[i] > mx) {
            mx = ir[i];
        }
    }

    int32_t pp = mx - mn;

    if (pp >= 800) {
        weak_pp_recover_streak = 0U;
        return;
    }

    if (mn < WEAK_PP_RAIL_CODE_LO || mx < WEAK_PP_RAIL_CODE_LO) {
        weak_pp_recover_streak = 0U;
        return;
    }

    weak_pp_recover_streak++;

    if (weak_pp_recover_streak < WEAK_PP_RECOVER_STREAK) {
        return;
    }

    weak_pp_recover_streak = 0U;

    uint8_t nu = (uint8_t)(led_pa_ir + LED_PA_STEP);

    if (nu > LED_PA_START) {
        nu = LED_PA_START;
    }

    int wr1 = max86140_write_reg(REG_LED1_PA, nu);
    int wr2 = 0;

#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_100_IRRED)
    wr2 = max86140_write_reg(REG_LED2_PA, nu);
#endif

    if (wr1 != 0 || wr2 != 0) {
        return;
    }

    led_pa_ir = nu;
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_PPG_100_IRRED)
    led_pa_red = nu;
#endif

    hr_reanchor_after_led_autogain();
}

/* ══════════════════════════════════════════════════════════════════════════
 * PUBLIC API
 * ══════════════════════════════════════════════════════════════════════════ */

#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_WRIST_PROFILE)
#ifndef CONFIG_VITABAND_MAX86140_HR_WRIST_BPM_OFFSET
#define CONFIG_VITABAND_MAX86140_HR_WRIST_BPM_OFFSET 0
#endif
#endif

static uint8_t hr_wrist_display_bpm(uint8_t hr)
{
#if IS_ENABLED(CONFIG_VITABAND_MAX86140_HR_WRIST_PROFILE)
    if (hr == 0U) {
        return 0U;
    }

    int32_t adj =
        (int32_t)hr + (int32_t)CONFIG_VITABAND_MAX86140_HR_WRIST_BPM_OFFSET;

    if (adj < (int32_t)MIN_HEART_RATE) {
        adj = (int32_t)MIN_HEART_RATE;
    }
    if (adj > (int32_t)MAX_HEART_RATE) {
        adj = (int32_t)MAX_HEART_RATE;
    }

    return (uint8_t)adj;
#else
    return hr;
#endif
}

uint8_t max86140_read_heartrate(void)
{
    uint8_t n = 0;
    int ret = max86140_read_fifo(&n, ir_samples);
    uint32_t t = k_uptime_get_32();
    static uint16_t empty_fifo_streak;

    if (ret != 0 || n == 0) {
        empty_fifo_streak++;
        if (empty_fifo_streak >= HR_EMPTY_FIFO_LOOPS) {
            hr_contact_lost_reset();
            empty_fifo_streak = 0U;
        }
        max86140_rtt_status(t, ret, false);
        return hr_wrist_display_bpm(calculated_hr);
    }

    empty_fifo_streak = 0U;

    uint32_t batch_tick = ir_sample_index;

    ir_sample_index += (uint32_t)n;

    max86140_calculate_hr(ir_samples, n, batch_tick);

    max86140_ir_autogain_rail_flat(ir_samples, n);
    max86140_ir_recover_near_floor(ir_samples, n);

    t = k_uptime_get_32();
    max86140_rtt_status(t, 0, true);

    return hr_wrist_display_bpm(calculated_hr);
}