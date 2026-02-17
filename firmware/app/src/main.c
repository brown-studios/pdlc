#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "control.h"
#include "pdlc.h"
#include "power_indicator.h"
#include "status_indicator.h"

LOG_MODULE_REGISTER(app);

#define WATCHDOG_TIMEOUT_MS (1000)
static const struct device *watchdog_dev = DEVICE_DT_GET(DT_NODELABEL(iwdg));

STATUS_INDICATOR_PATTERN_LOOP(status_indicator_pattern_generic_error, 0,
    STATUS_INDICATOR_PATTERN_ENTRY(STATUS_INDICATOR_HUE_RED, 2, 2, 1));

STATUS_INDICATOR_PATTERN_LOOP(status_indicator_pattern_control_error, 4,
    STATUS_INDICATOR_PATTERN_ENTRY(STATUS_INDICATOR_HUE_MAGENTA, 1, 3, STATUS_INDICATOR_PATTERN_PARAM_CYCLES));

STATUS_INDICATOR_PATTERN_LOOP(status_indicator_pattern_pdlc_error, 4,
    STATUS_INDICATOR_PATTERN_ENTRY(STATUS_INDICATOR_HUE_RED, 1, 3, STATUS_INDICATOR_PATTERN_PARAM_CYCLES));

static void show_error(int err) {
    switch (ERROR_CATEGORY(err)) {
        case ERROR_CATEGORY(CONTROL_ERROR_FIRST_):
            status_indicator_pattern(status_indicator_pattern_control_error, ERROR_INDEX(err));
            break;
        case ERROR_CATEGORY(PDLC_ERROR_FIRST_):
            status_indicator_pattern(status_indicator_pattern_pdlc_error, ERROR_INDEX(err));
            break;
        default:
            status_indicator_pattern(status_indicator_pattern_generic_error, 0);
            break;
    }
}

static void show_status(struct pdlc_status status) {
    switch (status.state) {
        case PDLC_STATE_IDLE:
            status_indicator_off();
            break;
        case PDLC_STATE_WAIT_FOR_HV_GOOD:
        case PDLC_STATE_WAIT_FOR_HV_STEADY:
        case PDLC_STATE_WAIT_FOR_OUTPUT_GOOD:
        case PDLC_STATE_WAIT_FOR_OUTPUT_STEADY:
            status_indicator_on(STATUS_INDICATOR_HUE_YELLOW);
            break;
        case PDLC_STATE_RUN:
            status_indicator_on(STATUS_INDICATOR_HUE_GREEN);
            break;
        case PDLC_STATE_HICCUP:
        case PDLC_STATE_FAULT:
            show_error(-status.error);
            break;
    }
}

static int setup(void) {
    int err;
    struct wdt_timeout_cfg watchdog_cfg = {
        .window = {
            .min = 0,
            .max = WATCHDOG_TIMEOUT_MS,
        },
        .callback = NULL,
        .flags = WDT_FLAG_RESET_SOC,
    };
    if ((err = wdt_install_timeout(watchdog_dev, &watchdog_cfg))) {
        return err;
    }
    if ((err = wdt_setup(watchdog_dev, WDT_OPT_PAUSE_HALTED_BY_DBG))) {
        return err;
    }
    if ((err = control_init())) {
        return err;
    }
    if ((err = pdlc_init())) {
        return err;
    }
    return 0;
}

static void loop(void) {
    static bool on;
    static enum control_action action_pending;
    enum control_action action = control_get_action();

    if (action != action_pending) {
        action_pending = action;
        switch (control_get_action()) {
            case CONTROL_ACTION_PRESS_MODE:
                LOG_INF("Control: CONTROL_ACTION_PRESS_MODE");
                on = !on;
                break;
            case CONTROL_ACTION_PRESS_SET:
                LOG_INF("Control: CONTROL_ACTION_PRESS_SET");
                break;
            case CONTROL_ACTION_LONG_HOLD_SET:
                LOG_INF("Control: CONTROL_ACTION_LONG_HOLD_SET");
                // todo: factory reset
                break;
            default:
                break;
        }
    }

    const unsigned scale = 256;
    const unsigned transition = 1000;
    struct pdlc_status status = pdlc_poll(on && control_get_enable() ? scale : 0, transition);
    show_status(status);
}

int main(void) {
    int err;
    if ((err = setup())) {
        status_indicator_on(STATUS_INDICATOR_HUE_RED);
        return err;
    }
    power_indicator_pulse();

    while (true) {
        if ((err = wdt_feed(watchdog_dev, 0))) {
            return err;
        }
        loop();
        k_msleep(10);
    }
    return 0;
}
