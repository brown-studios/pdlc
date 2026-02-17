/**
 * @file
 * @brief Reads the control button state.
 */

#pragma once

#include <stdint.h>

enum control_action {
    CONTROL_ACTION_RELEASE = 0,
    CONTROL_ACTION_PRESS_MODE = 1,
    CONTROL_ACTION_PRESS_SET = 2,
    CONTROL_ACTION_LONG_HOLD_SET = 3,
};

/**
 * @brief Get the action determined by the buttons.
 */
enum control_action control_get_action(void);

/**
 * @brief Get whether the output is enabled.
 */
bool control_get_enable(void);

/**
 * @brief Initialize the control driver.
 */
int control_init(void);
