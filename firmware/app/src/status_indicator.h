/**
 * @file
 * @brief Sets the status indicator color.
 */

#pragma once

#include <stdint.h>

#define STATUS_INDICATOR_HUE_RED (0)
#define STATUS_INDICATOR_HUE_AMBER (5462)
#define STATUS_INDICATOR_HUE_YELLOW (10923)
#define STATUS_INDICATOR_HUE_GREEN (21845)
#define STATUS_INDICATOR_HUE_CYAN (32768)
#define STATUS_INDICATOR_HUE_BLUE (43691)
#define STATUS_INDICATOR_HUE_MAGENTA (54613)

/**
 * @brief Turn the indicator off.
 */
void status_indicator_off(void);

/**
 * @brief Turn the indicator on.
 *
 * @param hue Color hue in a range from 0 to 65536, cycles every 65536.
 */
void status_indicator_on(unsigned hue);

/**
 * @brief Defines a pattern of colors for an indicator.
 *
 * The indicator turns on with the given hue for on_time then off for off_time blinking up to cycle times.
 * The time intervals are specified in 0.1 second increments.
 * Terminate an array of pattern entries with an entry with cycles = 0.
 * If the last entry has zero on_time then the pattern repeats after off_time interval, otherwise it stops.
 * If the value of cycles is STATUS_INDICATOR_PATTERN_PARAM_CYCLES then the pattern parameter determines the count.
 */
struct status_indicator_pattern_entry {
    unsigned hue : 16;
    unsigned on_time : 5;
    unsigned off_time : 5;
    unsigned cycles : 6;
};
_Static_assert(sizeof(struct status_indicator_pattern_entry) == 4, "");
typedef struct status_indicator_pattern_entry status_indicator_pattern_t;

#define STATUS_INDICATOR_PATTERN_PARAM_CYCLES (0x3f)
#define STATUS_INDICATOR_PATTERN_ENTRY(hue_, on_time_, off_time_, cycles_) { .hue = hue_, .on_time = on_time_, .off_time = off_time_, .cycles = cycles_ }
#define STATUS_INDICATOR_PATTERN_ENTRY_REPEAT(delay_) STATUS_INDICATOR_PATTERN_ENTRY(0, 0, delay_, 0)
#define STATUS_INDICATOR_PATTERN_ENTRY_END STATUS_INDICATOR_PATTERN_ENTRY(0, 1, 0, 0)
#define STATUS_INDICATOR_PATTERN_ONCE(name, ...) static const status_indicator_pattern_t name[] = { __VA_ARGS__, STATUS_INDICATOR_PATTERN_ENTRY_END }
#define STATUS_INDICATOR_PATTERN_LOOP(name, delay_, ...) static const status_indicator_pattern_t name[] = { __VA_ARGS__, STATUS_INDICATOR_PATTERN_ENTRY_REPEAT(delay_) }

/**
 * @brief Perform an indicator pattern.
 *
 * The pattern remains active until status_indicator_on or status_indicator_off are called.
 * Calling this function repeatedly with the same pattern does not restart it.
 *
 * @param pattern Pattern to perform, or NULL to stop the pattern at the end
 * its current on-time cycle or turn the indicator off immediately if no pattern
 * is running.
 * @param param Parameter value for STATUS_INDICATOR_PATTERN_PARAM_CYCLES, 0 if unused.
 */
void status_indicator_pattern(const status_indicator_pattern_t *pattern, unsigned cycles);
