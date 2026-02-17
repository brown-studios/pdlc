#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include "power_indicator.h"

static const struct pwm_dt_spec power_pwm = PWM_DT_SPEC_GET_BY_IDX(DT_NODELABEL(power_indicator), 0);

static void power_indicator_handler(struct k_work* work);
K_WORK_DELAYABLE_DEFINE(power_indicator_work, power_indicator_handler);

#define POWER_INDICATOR_TIMEOUT_MS (10000)

// Brightness scale factors: 0 (off), 64 (full bright)
#define POWER_INDICATOR_SCALE (32)
#define POWER_INDICATOR_SCALE_BITS (6)

static void power_indicator_handler(struct k_work* work) {
    pwm_set_pulse_dt(&power_pwm, 0);
}

void power_indicator_pulse(void) {
    pwm_set_pulse_dt(&power_pwm, (power_pwm.period * POWER_INDICATOR_SCALE) >> POWER_INDICATOR_SCALE_BITS);
    k_work_reschedule(&power_indicator_work, K_MSEC(POWER_INDICATOR_TIMEOUT_MS));
}
