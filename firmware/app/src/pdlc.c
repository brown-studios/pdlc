#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stm32l1xx_hal.h>
#include <stm32l1xx_ll_dac.h>
#include <stm32l1xx_ll_dma.h>
#include <stm32l1xx_ll_tim.h>
#include <zephyr/cache.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/dma.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include "pdlc.h"
#include <pdlc_tables.h>

LOG_MODULE_REGISTER(pdlc);

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)

// When 1, generates the waveform but doesn't turn on the output.
#define PDLC_DEBUG_DISABLE_OUTPUT 0

// When between 0 and 256, forces the output scale factor to this value.
#define PDLC_DEBUG_FORCE_OUTPUT_SCALE -1

// When 1, waits indefinitely for HV_PG to be asserted.
#define PDLC_DEBUG_WAIT_FOR_HV_GOOD_INDEFINITELY 0

// When 1, waits indefinitely after HV_PG is asserted, prevents the output from being enabled.
#define PDLC_DEBUG_WAIT_FOR_HV_STEADY_INDEFINITELY 0

// When 1, waits indefinitely for OUTPUT_PG to be asserted.
#define PDLC_DEBUG_WAIT_FOR_OUTPUT_GOOD_INDEFINITELY 0

// When 1, waits indefinitely after OUTPUT_PG is asserted, prevents entry into the PDLC_STATE_RUN.
#define PDLC_DEBUG_WAIT_FOR_OUTPUT_STEADY_INDEFINITELY 0

/*
 * OUTPUT ELECTRICAL PARAMETERS
 */

struct pdlc_output_config {
    // The AC output frequency in Hz.
    unsigned freq;
    // The minimum DC output voltage in mV.
    unsigned voltage_min;
    // The maximum DC output voltage in mV.
    unsigned voltage_max;
};
static const struct pdlc_output_config PDLC_OUTPUT_CONFIG_DEFAULT = {
    .freq = 100,
    .voltage_min = 3000,
    .voltage_max = 60000,
};

// Calibration for the buck converters, linear DAC code to output voltage in millivolts.
#define PDLC_MIN_ADJ_CODE (0x100)
#define PDLC_MAX_ADJ_CODE (0xf00)

// Theoretical calibration
// #define PDLC_LOW_ADJ_CODE (383)
// #define PDLC_LOW_ADJ_VOLTAGE (63010)
// #define PDLC_HIGH_ADJ_CODE (3769)
// #define PDLC_HIGH_ADJ_VOLTAGE (2990)

// Actual calibration
// In these tests, phase B adj voltage was 0.008 V higher than phase A
#define PDLC_LOW_ADJ_CODE (500)
#define PDLC_LOW_ADJ_VOLTAGE (60560) // phase A: 60560, phase B: 61130
#define PDLC_HIGH_ADJ_CODE (3500)
#define PDLC_HIGH_ADJ_VOLTAGE (7730) // phase A: 7730, phase B: 7870

static inline unsigned pdlc_voltage_to_dac_code(unsigned voltage) {
    int dac_code = ((int)voltage - PDLC_HIGH_ADJ_VOLTAGE) * (PDLC_HIGH_ADJ_CODE - PDLC_LOW_ADJ_CODE) /
            (PDLC_HIGH_ADJ_VOLTAGE - PDLC_LOW_ADJ_VOLTAGE) + PDLC_HIGH_ADJ_CODE;
    return MIN(MAX(dac_code, PDLC_MIN_ADJ_CODE), PDLC_MAX_ADJ_CODE);
}

/*
 * PDLC WAVEFORM GENERATOR
 */

// Number of samples in the output waveform.
#define PDLC_HALF_WAVE_SAMPLES (PDLC_QUARTER_WAVE_SAMPLES * 2)
#define PDLC_FULL_WAVE_SAMPLES (PDLC_QUARTER_WAVE_SAMPLES * 4)

#define PDLC_WAVE_TIMER_NUMBER 6
#define PDLC_WAVE_TIMER _CONCAT(TIM, PDLC_WAVE_TIMER_NUMBER)
static const struct device *pdlc_wave_timer_dev = DEVICE_DT_GET(DT_NODELABEL(pdlc_wave_timer));

#define PDLC_DAC DAC1
static const struct device *const pdlc_dac_dev = DEVICE_DT_GET(DT_PHANDLE(ZEPHYR_USER_NODE, pdlc_dac));

#define PDLC_DMA DMA1
#define PDLC_DMA_CHANNEL (2) // DMA channel 2 serves DAC1_CH1 / TIM6
static const struct device *const pdlc_dma_dev = DEVICE_DT_GET(DT_PHANDLE(ZEPHYR_USER_NODE, pdlc_dma));

// Unsigned 24.8 bit fixed point value.
typedef uint32_t pdlc_dac_amplitude_frac;
#define PDLC_DAC_AMPLITUDE_FRAC_SHIFT (8)

struct pdlc_wave_data {
    // Guards access to the structure.
    // Functions that require holding this spinlock have the `_wl` suffix.
    struct k_spinlock lock;
    // The frequency of the waveform in Hz.
    unsigned freq;
    // True if an error occurred during DMA.
    bool dma_error;
    // The waveform's DAC DC bias level.
    unsigned dac_bias;
    // The waveform's DAC maximum amplitude at full scale.
    unsigned dac_amplitude_max;
    // The waveform's DAC peak-to-peak amplitude, expressed as a 12.8 fixed point fraction for interpolation.
    pdlc_dac_amplitude_frac dac_amplitude_frac;
    // The waveform's DAC peak-to-peak amplitude target.
    pdlc_dac_amplitude_frac dac_amplitude_frac_target;
    // The slew rate at which to increase or decrease the amplitude in code units per half electrical cycle, 0 if infinite.
    pdlc_dac_amplitude_frac dac_amplitude_frac_ramp;
    // The double-buffered output waveform, each word contains a 12-bit right aligned value for each channel.
    // Half of the waveform is updated with current scaling factors at each zero crossing when the DMA reports
    // that the transfer is half-complete or complete (and cycling back).
    uint32_t dac_samples[PDLC_FULL_WAVE_SAMPLES];
};
static struct pdlc_wave_data pdlc_wave_data;

static inline uint32_t pdlc_dac_encode(uint32_t channel_1, uint32_t channel_2) {
    return (channel_1 << 16) | channel_2;
}

// Generates a complementary pair of DAC waveforms in the first half of the buffer.
static void pdlc_wave_phase_1_wl(void) {
    const unsigned dac_bias = pdlc_wave_data.dac_bias;
    const unsigned dac_amplitude = pdlc_wave_data.dac_amplitude_frac >> PDLC_DAC_AMPLITUDE_FRAC_SHIFT;
    uint32_t * const dac_samples = pdlc_wave_data.dac_samples;
    for (unsigned i = 0; i <= PDLC_QUARTER_WAVE_SAMPLES; i++) {
        unsigned dac_level = (pdlc_quarter_wave_table[i] * dac_amplitude) >> 16;
        dac_samples[i] = pdlc_dac_encode(dac_bias + dac_level, dac_bias - dac_level);
    }
    for (unsigned i = 1; i < PDLC_QUARTER_WAVE_SAMPLES; i++) {
        dac_samples[PDLC_QUARTER_WAVE_SAMPLES + i] = dac_samples[PDLC_QUARTER_WAVE_SAMPLES - i];
    }
    sys_cache_data_flush_range(&dac_samples[0], sizeof(uint32_t) * PDLC_HALF_WAVE_SAMPLES);
}

// Generates a complementary pair of DAC waveforms in the second half of the buffer.
static void pdlc_wave_phase_2_wl(void) {
    const unsigned dac_bias = pdlc_wave_data.dac_bias;
    const unsigned dac_amplitude = pdlc_wave_data.dac_amplitude_frac >> PDLC_DAC_AMPLITUDE_FRAC_SHIFT;
    uint32_t * const dac_samples = pdlc_wave_data.dac_samples;
    for (unsigned i = 0; i <= PDLC_QUARTER_WAVE_SAMPLES; i++) {
        unsigned dac_level = (pdlc_quarter_wave_table[i] * dac_amplitude) >> 16;
        dac_samples[PDLC_HALF_WAVE_SAMPLES + i] = pdlc_dac_encode(dac_bias - dac_level, dac_bias + dac_level);
    }
    for (unsigned i = 1; i < PDLC_QUARTER_WAVE_SAMPLES; i++) {
        dac_samples[PDLC_HALF_WAVE_SAMPLES + PDLC_QUARTER_WAVE_SAMPLES + i] =
                dac_samples[PDLC_HALF_WAVE_SAMPLES + PDLC_QUARTER_WAVE_SAMPLES - i];
    }
    sys_cache_data_flush_range(&dac_samples[PDLC_HALF_WAVE_SAMPLES], sizeof(uint32_t) * PDLC_HALF_WAVE_SAMPLES);
}

// Updates the amplitude based on the ramp.
static void pdlc_wave_ramp_amplitude_wl(void) {
    if (pdlc_wave_data.dac_amplitude_frac_ramp) {
        if (pdlc_wave_data.dac_amplitude_frac < pdlc_wave_data.dac_amplitude_frac_target) {
            pdlc_wave_data.dac_amplitude_frac += MIN(pdlc_wave_data.dac_amplitude_frac_target - pdlc_wave_data.dac_amplitude_frac,
                    pdlc_wave_data.dac_amplitude_frac_ramp);
        } else if (pdlc_wave_data.dac_amplitude_frac > pdlc_wave_data.dac_amplitude_frac_target) {
            pdlc_wave_data.dac_amplitude_frac -= MIN(pdlc_wave_data.dac_amplitude_frac - pdlc_wave_data.dac_amplitude_frac_target,
                    pdlc_wave_data.dac_amplitude_frac_ramp);
        }
    } else {
        pdlc_wave_data.dac_amplitude_frac = pdlc_wave_data.dac_amplitude_frac_target;
    }
}

static void pdlc_dma_callback(const struct device *dev, void *user_data, uint32_t channel, int status) {
    K_SPINLOCK(&pdlc_wave_data.lock) {
        switch (status) {
            case DMA_STATUS_BLOCK:
                pdlc_wave_ramp_amplitude_wl();
                pdlc_wave_phase_1_wl();
                break;
            case DMA_STATUS_COMPLETE:
                pdlc_wave_ramp_amplitude_wl();
                pdlc_wave_phase_2_wl();
                break;
            default:
                pdlc_wave_data.dma_error = true;
                break;
        }
    }
}

// Writes the current bias to the DAC data holding register.
// Must only be called when the DAC trigger function is disabled.
static void pdlc_wave_write_bias_wl(void) {
    LL_DAC_ConvertDualData12RightAligned(PDLC_DAC, pdlc_wave_data.dac_bias, pdlc_wave_data.dac_bias);
    // dac_write_value(pdlc_dac_dev, 1, pdlc_wave_data.dac_bias);
    // dac_write_value(pdlc_dac_dev, 2, pdlc_wave_data.dac_bias);
}

// Starts the waveform at zero amplitude.
// Call pdlc_wave_init() beforehand to setup the parameters.
static int pdlc_wave_start(const struct pdlc_output_config *cfg) {
    const unsigned dac_at_vmax = pdlc_voltage_to_dac_code(cfg->voltage_max);
    const unsigned dac_at_vmin = pdlc_voltage_to_dac_code(cfg->voltage_min);
    const unsigned bias = (dac_at_vmax + dac_at_vmin) / 2;
    const unsigned amplitude_max = dac_at_vmin - dac_at_vmax;
    LOG_INF("pdlc_wave_start: vmax=%u, vmin=%u, dac_at_vmax=%u, dac_at_vmin=%u, bias=%u, amplitude_max=%u", cfg->voltage_max, cfg->voltage_min, dac_at_vmax, dac_at_vmin, bias, amplitude_max);

    K_SPINLOCK(&pdlc_wave_data.lock) {
        pdlc_wave_data.freq = cfg->freq;
        pdlc_wave_data.dma_error = false;
        pdlc_wave_data.dac_bias = bias;
        pdlc_wave_data.dac_amplitude_max = amplitude_max;
        pdlc_wave_data.dac_amplitude_frac = 0;
        pdlc_wave_data.dac_amplitude_frac_target = 0;
        pdlc_wave_data.dac_amplitude_frac_ramp = 0;
        for (unsigned i = 0; i < PDLC_FULL_WAVE_SAMPLES; i++) {
            pdlc_wave_data.dac_samples[i] = pdlc_dac_encode(bias, bias);
        }
        pdlc_wave_write_bias_wl();
    }

    // Enable DMA request for one DAC channel; the same request fills both channels.
    LL_DAC_EnableTrigger(PDLC_DAC, LL_DAC_CHANNEL_1);
    LL_DAC_EnableTrigger(PDLC_DAC, LL_DAC_CHANNEL_2);
    LL_DAC_EnableDMAReq(PDLC_DAC, LL_DAC_CHANNEL_1);

    int err;
    struct dma_block_config pdlc_dma_block_config = {
        .source_address = (uint32_t)pdlc_wave_data.dac_samples,
        .dest_address = (uint32_t)&PDLC_DAC->DHR12RD, // DAC dual data 12-bit right aligned holding register
        .block_size = sizeof(pdlc_wave_data.dac_samples),
        .source_addr_adj = DMA_ADDR_ADJ_INCREMENT,
        .dest_addr_adj = DMA_ADDR_ADJ_NO_CHANGE,
        .source_reload_en = true,
        .dest_reload_en = true,
    };
    struct dma_config pdlc_dma_config = {
        .channel_direction = MEMORY_TO_PERIPHERAL,
        .channel_priority = 2, // HIGH (not that it matters right now because there is no contention)
        .complete_callback_en = true,
        .source_data_size = 4,
        .dest_data_size = 4,
        .block_count = 1,
        .head_block = &pdlc_dma_block_config,
        .dma_callback = pdlc_dma_callback,
    };
    if ((err = dma_config(pdlc_dma_dev, PDLC_DMA_CHANNEL, &pdlc_dma_config))) {
        return err;
    }
    if ((err = dma_start(pdlc_dma_dev, PDLC_DMA_CHANNEL))) {
        LOG_ERR("pdlc_wave_start: dma_start error %d", err);
        return err;
    }

    struct counter_top_cfg counter_top_cfg = {
        .ticks = counter_get_frequency(pdlc_wave_timer_dev) / (cfg->freq * PDLC_FULL_WAVE_SAMPLES),
    };
    if ((err = counter_set_top_value(pdlc_wave_timer_dev, &counter_top_cfg))) {
        LOG_ERR("pdlc_wave_start: counter_set_top_value error %d", err);
        return err;
    }
    if ((err = counter_start(pdlc_wave_timer_dev))) {
        LOG_ERR("pdlc_wave_start: counter_start error %d", err);
        return err;
    }
    return 0;
}

// Stops the waveform.
static void pdlc_wave_stop(void) {
    LL_DAC_DisableDMAReq(PDLC_DAC, LL_DAC_CHANNEL_1);
    LL_DAC_DisableTrigger(PDLC_DAC, LL_DAC_CHANNEL_1);
    LL_DAC_DisableTrigger(PDLC_DAC, LL_DAC_CHANNEL_2);
    LL_DAC_ClearFlag_DMAUDR1(PDLC_DAC);

    int err;
    if ((err = counter_stop(pdlc_wave_timer_dev))) {
        LOG_ERR("pdlc_wave_stop: counter_stop error %d", err);
    }
    if ((err = dma_stop(pdlc_dma_dev, PDLC_DMA_CHANNEL))) {
        LOG_ERR("pdlc_wave_stop: dma_stop error %d", err);
    }

    K_SPINLOCK(&pdlc_wave_data.lock) {
        pdlc_wave_write_bias_wl();
    }
}

// Updates the waveform's target and retrieves the DMA status while it is running.
// Returns 0 if ready, 1 if a transition is in progress, -1 if an error occurred.
static int pdlc_wave_update(unsigned scale, unsigned transition) {
    int status;
    K_SPINLOCK(&pdlc_wave_data.lock) {
        _Static_assert(PDLC_DAC_AMPLITUDE_FRAC_SHIFT == 8, ""); // assume same as scale so no shift needed below
#if PDLC_DEBUG_FORCE_OUTPUT_SCALE >= 0 && PDLC_DEBUG_FORCE_OUTPUT_SCALE <= 256
        scale = PDLC_DEBUG_FORCE_OUTPUT_SCALE;
#endif
        pdlc_wave_data.dac_amplitude_frac_target = pdlc_wave_data.dac_amplitude_max * scale;
        pdlc_wave_data.dac_amplitude_frac_ramp = transition ?
                ((pdlc_wave_data.dac_amplitude_max * 1000) << PDLC_DAC_AMPLITUDE_FRAC_SHIFT) /
                (transition * pdlc_wave_data.freq) : 0;

        if (!pdlc_wave_data.dma_error && LL_DAC_IsActiveFlag_DMAUDR1(PDLC_DAC)) {
            // Detected a DMA underrun.
            LOG_ERR("pdlc_wave_update: DAC DMA underrun");
            pdlc_wave_data.dma_error = true;
        }
        status = pdlc_wave_data.dma_error ? -1 :
                pdlc_wave_data.dac_amplitude_frac != pdlc_wave_data.dac_amplitude_frac_target ? 1 : 0;
    }
    return status;
}

/*
 * PDLC LOOP STATE MACHINE
 */

static const struct gpio_dt_spec pdlc_hv_en_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, pdlc_hv_en_gpios);
static const struct gpio_dt_spec pdlc_hv_pg_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, pdlc_hv_pg_gpios);
static const struct gpio_dt_spec pdlc_output_en_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, pdlc_output_en_gpios);
static const struct gpio_dt_spec pdlc_output_pg_a_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, pdlc_output_pg_a_gpios);
static const struct gpio_dt_spec pdlc_output_pg_b_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, pdlc_output_pg_b_gpios);

#define MS_TO_CYCLES(ms) (uint32_t)((uint64_t)CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC * (ms) / 1000)

// Duration to wait for HV_PG to be asserted.
#define PDLC_HV_GOOD_MS (100)
#define PDLC_HV_GOOD_CYCLES (MS_TO_CYCLES(PDLC_HV_GOOD_MS))

// Duration to wait after HV_PG has been asserted before declaring the supply is steady.
#define PDLC_HV_STEADY_MS (100)
#define PDLC_HV_STEADY_CYCLES (MS_TO_CYCLES(PDLC_HV_STEADY_MS))

// Duration to wait for PDLC_PG_A and PDLC_PG_B to be asserted.
#define PDLC_OUTPUT_GOOD_MS (100)
#define PDLC_OUTPUT_GOOD_CYCLES (MS_TO_CYCLES(PDLC_OUTPUT_GOOD_MS))

// Duration to wait after PDLC_PG_A and PDLC_PG_B have been asserted before declaring the supply is steady
// and clearing the hiccup counter.
#define PDLC_OUTPUT_STEADY_MS (2000)
#define PDLC_OUTPUT_STEADY_CYCLES (MS_TO_CYCLES(PDLC_OUTPUT_STEADY_MS))

// Duration to wait after a hiccup occurs before attempting to operate the PDLC panel again
// and the maximum number of consecutive retries before entering a fault condition.
#define PDLC_HICCUP_COOLDOWN_MS (2000)
#define PDLC_HICCUP_COOLDOWN_CYCLES (MS_TO_CYCLES(PDLC_HICCUP_COOLDOWN_MS))
#define PDLC_HICCUP_MAX_COUNT (3)

struct pdlc_loop_data {
    // Guards access to the structure.
    // Functions that require holding this spinlock have the `_ll` suffix.
    struct k_spinlock lock;
    // The state of the driver.
    enum pdlc_state state;
    // Cycle time when the driver entered the current state, used for timing wait conditions.
    uint32_t state_entry_time;
    // Error code of the most recent hiccup or fault.
    int error;
    // The number of consecutive hiccups.
    unsigned hiccup_count;
    // The output waveform configuration.
    struct pdlc_output_config output_config;
    // The output scale factor, 0 (off) to 256 (full on).
    unsigned output_scale;
    // The output transition time from off to full-on in milliseconds.
    unsigned output_transition;
};
static struct pdlc_loop_data pdlc_loop_data = {
    .output_config = PDLC_OUTPUT_CONFIG_DEFAULT,
};

static void pdlc_update_ll(void) {
#if !PDLC_DEBUG_DISABLE_OUTPUT
    const bool hv_pg = gpio_pin_get_dt(&pdlc_hv_pg_gpio);
    gpio_port_value_t pdlc_output_pg_value = 0;
    gpio_port_get(pdlc_output_pg_a_gpio.port, &pdlc_output_pg_value);
    const bool pdlc_output_pg_a = IS_BIT_SET(pdlc_output_pg_value, pdlc_output_pg_a_gpio.pin);
    const bool pdlc_output_pg_b = IS_BIT_SET(pdlc_output_pg_value, pdlc_output_pg_b_gpio.pin);
#endif
    const uint32_t cycle_time = k_cycle_get_32();
    int err;

    #define ENTER_STATE(state_) \
        pdlc_loop_data.state = state_; \
        pdlc_loop_data.state_entry_time = cycle_time; \
        LOG_INF("State: " # state_);

    #define GOTO_HICCUP(error_) \
        pdlc_loop_data.error = error_; \
        LOG_ERR("Hiccup: " # error_); \
        goto hiccup;

    if (pdlc_loop_data.output_scale) {
        if (pdlc_loop_data.state == PDLC_STATE_FAULT) {
            return; // do not operate the output until the fault is cleared
        }
        if (pdlc_loop_data.state == PDLC_STATE_HICCUP) {
            if (cycle_time - pdlc_loop_data.state_entry_time < PDLC_HICCUP_COOLDOWN_CYCLES) {
                return; // wait longer
            }
            ENTER_STATE(PDLC_STATE_IDLE);
        }
#if PDLC_DEBUG_DISABLE_OUTPUT
        if (pdlc_loop_data.state == PDLC_STATE_IDLE) {
            if ((err = pdlc_wave_start(&pdlc_loop_data.output_config))) {
                GOTO_HICCUP(PDLC_ERROR_DRIVER);
            }
            ENTER_STATE(PDLC_STATE_WAIT_FOR_OUTPUT_STEADY);
        }
#else
        if (pdlc_loop_data.state == PDLC_STATE_IDLE) {
            gpio_pin_set_dt(&pdlc_hv_en_gpio, true);
            ENTER_STATE(PDLC_STATE_WAIT_FOR_HV_GOOD);
        }
        if (pdlc_loop_data.state == PDLC_STATE_WAIT_FOR_HV_GOOD) {
            if (hv_pg) {
                ENTER_STATE(PDLC_STATE_WAIT_FOR_HV_STEADY);
            } else if (cycle_time - pdlc_loop_data.state_entry_time < PDLC_HV_GOOD_CYCLES || PDLC_DEBUG_WAIT_FOR_HV_GOOD_INDEFINITELY) {
                return; // wait longer
            } else {
                GOTO_HICCUP(PDLC_ERROR_HV_TIMEOUT);
            }
        }
        if (!hv_pg && pdlc_loop_data.state != PDLC_STATE_WAIT_FOR_OUTPUT_GOOD) {
            // Ignore HV fault while the output is being turned on because the sudden current rush
            // to charge the output capacitors can overwhelm the HV supply momentarily
            GOTO_HICCUP(PDLC_ERROR_HV_FAULT);
        }
        if (pdlc_loop_data.state == PDLC_STATE_WAIT_FOR_HV_STEADY) {
            if (cycle_time - pdlc_loop_data.state_entry_time < PDLC_HV_STEADY_CYCLES || PDLC_DEBUG_WAIT_FOR_HV_STEADY_INDEFINITELY) {
                return; // wait longer
            }
            if ((err = pdlc_wave_start(&pdlc_loop_data.output_config))) {
                GOTO_HICCUP(PDLC_ERROR_DRIVER);
            }
            gpio_pin_set_dt(&pdlc_output_en_gpio, true);
            ENTER_STATE(PDLC_STATE_WAIT_FOR_OUTPUT_GOOD);
        }
        if (pdlc_loop_data.state == PDLC_STATE_WAIT_FOR_OUTPUT_GOOD) {
            if (pdlc_output_pg_a && pdlc_output_pg_b) {
                ENTER_STATE(PDLC_STATE_WAIT_FOR_OUTPUT_STEADY);
            } else if (cycle_time - pdlc_loop_data.state_entry_time < PDLC_OUTPUT_GOOD_CYCLES || PDLC_DEBUG_WAIT_FOR_OUTPUT_GOOD_INDEFINITELY) {
                return; // wait longer
            } else {
                GOTO_HICCUP(PDLC_ERROR_OUTPUT_TIMEOUT);
            }
        }
        if (!pdlc_output_pg_a || !pdlc_output_pg_b) {
            GOTO_HICCUP(PDLC_ERROR_OUTPUT_FAULT);
        }
#endif
        if (pdlc_wave_update(pdlc_loop_data.output_scale, pdlc_loop_data.output_transition) < 0) {
            GOTO_HICCUP(PDLC_ERROR_DRIVER);
        }
        if (pdlc_loop_data.state == PDLC_STATE_WAIT_FOR_OUTPUT_STEADY) {
            if (cycle_time - pdlc_loop_data.state_entry_time < PDLC_OUTPUT_STEADY_CYCLES || PDLC_DEBUG_WAIT_FOR_OUTPUT_STEADY_INDEFINITELY) {
                return; // wait longer
            }
            ENTER_STATE(PDLC_STATE_RUN);
            pdlc_loop_data.hiccup_count = 0;
        }
        __ASSERT(pdlc_loop_data.state == PDLC_STATE_RUN, "expected PDLC run state");
        return; // everything is ok!

hiccup:
        gpio_pin_set_dt(&pdlc_output_en_gpio, false);
        gpio_pin_set_dt(&pdlc_hv_en_gpio, false);
        pdlc_wave_stop();
        ENTER_STATE(PDLC_STATE_HICCUP);
        pdlc_loop_data.hiccup_count += 1;
        if (pdlc_loop_data.hiccup_count < PDLC_HICCUP_MAX_COUNT && pdlc_loop_data.error != PDLC_ERROR_DRIVER) {
            return; // allow another retry
        }
        ENTER_STATE(PDLC_STATE_FAULT);
        return; // give up
    }

    // Output is disabled, reset state once transition is complete.
    if (pdlc_loop_data.state == PDLC_STATE_WAIT_FOR_OUTPUT_GOOD ||
            pdlc_loop_data.state == PDLC_STATE_WAIT_FOR_OUTPUT_STEADY ||
            pdlc_loop_data.state == PDLC_STATE_RUN) {
        if (hv_pg && pdlc_output_pg_a && pdlc_output_pg_b &&
                pdlc_wave_update(/*scale*/ 0, pdlc_loop_data.output_transition) > 0) {
            return ; // wait for transition to complete
        }
    }
    if (pdlc_loop_data.state != PDLC_STATE_IDLE) {
        gpio_pin_set_dt(&pdlc_output_en_gpio, false);
        gpio_pin_set_dt(&pdlc_hv_en_gpio, false);
        pdlc_wave_stop();
        ENTER_STATE(PDLC_STATE_IDLE);
        pdlc_loop_data.hiccup_count = 0;
    }
}

int pdlc_init(void) {
    int err;
    const struct device *pdlc_output_pg_port = pdlc_output_pg_a_gpio.port;
    if (pdlc_output_pg_port != pdlc_output_pg_b_gpio.port) {
        return -EIO;
    }
    if (!device_is_ready(pdlc_output_pg_port) ||
            !device_is_ready(pdlc_hv_en_gpio.port) ||
            !device_is_ready(pdlc_hv_pg_gpio.port) ||
            !device_is_ready(pdlc_output_en_gpio.port) ||
            !device_is_ready(pdlc_wave_timer_dev) ||
            !device_is_ready(pdlc_dac_dev) ||
            !device_is_ready(pdlc_dma_dev)) {
        return -ENODEV;
    }

    // Set up GPIOs.
    if ((err = gpio_pin_configure_dt(&pdlc_hv_en_gpio, GPIO_OUTPUT_INACTIVE)) ||
            (err = gpio_pin_configure_dt(&pdlc_hv_pg_gpio, GPIO_INPUT)) ||
            (err = gpio_pin_configure_dt(&pdlc_output_en_gpio, GPIO_OUTPUT_INACTIVE)) ||
            (err = gpio_pin_configure_dt(&pdlc_output_pg_a_gpio, GPIO_INPUT)) ||
            (err = gpio_pin_configure_dt(&pdlc_output_pg_b_gpio, GPIO_INPUT))) {
        return err;
    }

    // Configure the timer to trigger the DAC.
    LL_TIM_SetTriggerOutput(PDLC_WAVE_TIMER, LL_TIM_TRGO_UPDATE);

    // Configure the DAC.
    LL_DAC_SetTriggerSource(PDLC_DAC, LL_DAC_CHANNEL_1, _CONCAT(_CONCAT(LL_DAC_TRIG_EXT_TIM, PDLC_WAVE_TIMER_NUMBER), _TRGO));
    LL_DAC_SetTriggerSource(PDLC_DAC, LL_DAC_CHANNEL_2, _CONCAT(_CONCAT(LL_DAC_TRIG_EXT_TIM, PDLC_WAVE_TIMER_NUMBER), _TRGO));
    struct dac_channel_cfg dac_channel_cfg = {
        .channel_id = 1,
        .resolution = 12,
        .buffered = true,
        .internal = false,
    };
    if ((err = dac_channel_setup(pdlc_dac_dev, &dac_channel_cfg))) {
        return err;
    }
    dac_channel_cfg.channel_id = 2;
    if ((err = dac_channel_setup(pdlc_dac_dev, &dac_channel_cfg))) {
        return err;
    }
    return 0;
}

struct pdlc_status pdlc_poll(unsigned scale, unsigned transition) {
    struct pdlc_status status = {};
    K_SPINLOCK(&pdlc_loop_data.lock) {
        //pdlc_loop_data.output_config = PDLC_OUTPUT_CONFIG_DEFAULT;
        pdlc_loop_data.output_scale = scale;
        pdlc_loop_data.output_transition = transition;
        pdlc_update_ll();

        status.state = pdlc_loop_data.state;
        status.error = -pdlc_loop_data.error;
        status.hiccup_count = pdlc_loop_data.hiccup_count;
    }
    return status;
}
