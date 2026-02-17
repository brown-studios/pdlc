/**
 * @file Component to indicate status with color and flashing patterns.
 */

#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "status_indicator.h"

static const struct pwm_dt_spec red_pwm = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(status_indicator_red), 0);
static const struct pwm_dt_spec green_pwm = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(status_indicator_green), 0);
static const struct pwm_dt_spec blue_pwm = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(status_indicator_blue), 0);

K_SEM_DEFINE(status_indicator_sem, 1, 1);

static const status_indicator_pattern_t *status_indicator_pattern_current;
static unsigned status_indicator_pattern_index;
static unsigned status_indicator_pattern_cycle;
static unsigned status_indicator_pattern_param;
static bool status_indicator_pattern_on;

static void status_indicator_pattern_handler(struct k_work* work);
K_WORK_DELAYABLE_DEFINE(status_indicator_pattern_work, status_indicator_pattern_handler);

// Brightness scale factors: 0 (off), 64 (full bright)
#define STATUS_INDICATOR_RED_SCALE (20)
#define STATUS_INDICATOR_GREEN_SCALE (8)
#define STATUS_INDICATOR_BLUE_SCALE (12)
#define STATUS_INDICATOR_SCALE_BITS (6)

static void status_indicator_pwm_set_off_l() {
    pwm_set_pulse_dt(&red_pwm, 0);
    pwm_set_pulse_dt(&green_pwm, 0);
    pwm_set_pulse_dt(&blue_pwm, 0);
}

static void status_indicator_pwm_set_on_l(unsigned hue) {
    unsigned r, g, b;
    hue = ((hue & 0xffff) * 1536 + 0x8000) >> 16;
    if (hue < 512) {
        if (hue < 256) {
            g = hue;
            r = 256;
        } else {
            r = 512 - hue;
            g = 256;
        }
        b = 0;
    } else if (hue < 1024) {
        if (hue < 768) {
            b = hue - 512;
            g = 256;
        } else {
            g = 1024 - hue;
            b = 256;
        }
        r = 0;
    } else {
        if (hue < 1280) {
            r = hue - 1024;
            b = 256;
        } else {
            b = 1536 - hue;
            r = 256;
        }
        g = 0;
    }

    pwm_set_pulse_dt(&red_pwm, (((r * red_pwm.period) >> 8) * STATUS_INDICATOR_RED_SCALE) >> STATUS_INDICATOR_SCALE_BITS);
    pwm_set_pulse_dt(&green_pwm, (((g * green_pwm.period) >> 8) * STATUS_INDICATOR_GREEN_SCALE) >> STATUS_INDICATOR_SCALE_BITS);
    pwm_set_pulse_dt(&blue_pwm, (((b * blue_pwm.period) >> 8) * STATUS_INDICATOR_BLUE_SCALE) >> STATUS_INDICATOR_SCALE_BITS);
}

static void status_indicator_pattern_advance_l(void) {
    for (;;) {
        const struct status_indicator_pattern_entry *entry = &status_indicator_pattern_current[status_indicator_pattern_index];
        if (!status_indicator_pattern_on) {
            if (entry->cycles) {
                status_indicator_pattern_on = true;
                status_indicator_pwm_set_on_l(entry->hue);
                k_work_reschedule(&status_indicator_pattern_work, K_MSEC(entry->on_time * 100));
                break; // wait for next cycle
            }
            if (entry->on_time) {
                break; // end pattern
            }
            status_indicator_pattern_index = 0; // repeat pattern
            status_indicator_pattern_cycle = 0;
            if (entry->off_time) {
                k_work_reschedule(&status_indicator_pattern_work, K_MSEC(entry->off_time * 100));
                break; // wait for delay between repetitions
            }
        } else {
            status_indicator_pattern_cycle += 1;
            unsigned cycles = (entry->cycles == STATUS_INDICATOR_PATTERN_PARAM_CYCLES ? status_indicator_pattern_param : entry->cycles);
            if (status_indicator_pattern_cycle >= cycles) {
                status_indicator_pattern_index += 1;
                status_indicator_pattern_cycle = 0;
            }
            status_indicator_pattern_on = false;
            if (entry->off_time) {
                status_indicator_pwm_set_off_l();
                k_work_reschedule(&status_indicator_pattern_work, K_MSEC(entry->off_time * 100));
                break; // wait for next cycle
            }
        }
    }
}

static void status_indicator_pattern_handler(struct k_work* work) {
    k_sem_take(&status_indicator_sem, K_FOREVER);
    if (status_indicator_pattern_current) {
        status_indicator_pattern_advance_l();
    } else if (status_indicator_pattern_on) {
        status_indicator_pwm_set_off_l();
    }
    k_sem_give(&status_indicator_sem);
}

void status_indicator_off(void) {
    k_sem_take(&status_indicator_sem, K_FOREVER);
    status_indicator_pattern_current = NULL;
    status_indicator_pattern_on = false;
    status_indicator_pwm_set_off_l();
    k_sem_give(&status_indicator_sem);
}

void status_indicator_on(unsigned hue) {
    k_sem_take(&status_indicator_sem, K_FOREVER);
    status_indicator_pattern_current = NULL;
    status_indicator_pattern_on = false;
    status_indicator_pwm_set_on_l(hue);
    k_sem_give(&status_indicator_sem);
}

void status_indicator_pattern(const status_indicator_pattern_t *pattern, unsigned param) {
    k_sem_take(&status_indicator_sem, K_FOREVER);
    if (status_indicator_pattern_current != pattern || status_indicator_pattern_param != param) {
        status_indicator_pattern_current = pattern;
        status_indicator_pattern_param = param;
        if (pattern) {
            status_indicator_pattern_index = 0;
            status_indicator_pattern_cycle = 0;
            status_indicator_pattern_on = false;
            status_indicator_pattern_advance_l();
        }
    } else if (!status_indicator_pattern_on) {
        status_indicator_pwm_set_off_l();
    }
    k_sem_give(&status_indicator_sem);
}