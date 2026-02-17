#include <errno.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include "control.h"

#define ZEPHYR_USER_NODE DT_PATH(zephyr_user)
static const struct gpio_dt_spec control_mode_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, control_mode_gpios);
static const struct gpio_dt_spec control_set_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, control_set_gpios);
static const struct gpio_dt_spec control_en_gpio = GPIO_DT_SPEC_GET(ZEPHYR_USER_NODE, control_en_gpios);

#define CONTROL_DEBOUNCE_MS (10)
#define CONTROL_LONG_HOLD_MS (10000)

static struct gpio_callback control_gpio_callback;

static atomic_t control_action_pending;
static atomic_t control_action_current;
static atomic_t control_enable_pending;
static atomic_t control_enable_current;

static void control_work_handler(struct k_work* work);
K_WORK_DELAYABLE_DEFINE(control_work, control_work_handler);

static void control_gpio_handler(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins) {
    gpio_port_value_t value = 0;
    gpio_port_get(port, &value);
    bool control_mode = IS_BIT_SET(value, control_mode_gpio.pin);
    bool control_set = IS_BIT_SET(value, control_set_gpio.pin);
    bool control_en = IS_BIT_SET(value, control_en_gpio.pin);
    enum control_action action;
    if (control_mode && !control_set) {
        action = CONTROL_ACTION_PRESS_MODE;
    } else if (control_set && !control_mode) {
        action = CONTROL_ACTION_PRESS_SET;
    } else {
        action = CONTROL_ACTION_RELEASE;
    }
    atomic_set(&control_action_pending, action);
    atomic_set(&control_enable_pending, control_en);
    k_work_reschedule(&control_work, K_MSEC(CONTROL_DEBOUNCE_MS));
}

static void control_work_handler(struct k_work* work) {
    enum control_action action = atomic_get(&control_action_pending);
    bool reschedule_for_hold;
    switch (action) {
        case CONTROL_ACTION_PRESS_SET:
            reschedule_for_hold = atomic_cas(&control_action_pending, action, CONTROL_ACTION_LONG_HOLD_SET);
            break;
        default:
            reschedule_for_hold = false;
            break;
    }
    if (reschedule_for_hold) {
        k_work_reschedule(&control_work, K_MSEC(CONTROL_LONG_HOLD_MS));
    }

    atomic_set(&control_action_current, action);
    atomic_set(&control_enable_current, atomic_get(&control_enable_pending));
}

int control_init(void) {
    int err;
    const struct device *port = control_mode_gpio.port;
    if (port != control_set_gpio.port || port != control_en_gpio.port) {
        return -EIO;
    }
    if (!device_is_ready(port)) {
        return -ENODEV;
    }
    gpio_init_callback(&control_gpio_callback, control_gpio_handler,
        BIT(control_set_gpio.pin) | BIT(control_mode_gpio.pin) | BIT(control_en_gpio.pin));

    if ((err = gpio_pin_configure_dt(&control_mode_gpio, GPIO_INPUT)) ||
            (err = gpio_pin_configure_dt(&control_set_gpio, GPIO_INPUT)) ||
            (err = gpio_pin_configure_dt(&control_en_gpio, GPIO_INPUT))) {
        return err;
    }
    if ((err = gpio_pin_interrupt_configure_dt(&control_mode_gpio, GPIO_INT_EDGE_BOTH)) ||
            (err = gpio_pin_interrupt_configure_dt(&control_set_gpio, GPIO_INT_EDGE_BOTH)) ||
            (err = gpio_pin_interrupt_configure_dt(&control_en_gpio, GPIO_INT_EDGE_BOTH))) {
        return err;
    }
    if ((err = gpio_add_callback(port, &control_gpio_callback))) {
        return err;
    }
    return 0;
}

enum control_action control_get_action(void) {
    return atomic_get(&control_action_current);
}

bool control_get_enable(void) {
    return atomic_get(&control_enable_current);
}
