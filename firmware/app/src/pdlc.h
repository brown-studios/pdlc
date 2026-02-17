/**
 * @file
 * @brief PDLC panel driver.
 */

#pragma once

#include <stdint.h>

#include "errors.h"

int pdlc_init(void);

enum pdlc_state {
    // The driver is off.
    PDLC_STATE_IDLE = 0,
    // The driver is turning on, waiting for the HV_PG signal to be asserted.
    PDLC_STATE_WAIT_FOR_HV_GOOD = 1,
    // The driver is turning on, waiting for the HV_PG signal to remain asserted for a sufficient duration.
    PDLC_STATE_WAIT_FOR_HV_STEADY = 2,
    // The driver is turning on, waiting for PDLC_PG_A and PDLC_PG_B signals to be asserted.
    PDLC_STATE_WAIT_FOR_OUTPUT_GOOD = 3,
    // The driver is on, waiting for the PDLC_PG_A and PDLC_PG_B signal to remain asserted for a sufficient duration.
    PDLC_STATE_WAIT_FOR_OUTPUT_STEADY = 4,
    // The driver is on and the output appears steady so the hiccup counter has been reset.
    PDLC_STATE_RUN = 5,
    // The driver is off because it encountered a transient problem and will retry after a cooldown period expires.
    PDLC_STATE_HICCUP = 6,
    // The driver is off because it encountered a durable problem and will not try again until the output is reset.
    PDLC_STATE_FAULT = 7,
};

// PDLC driver status.
struct __packed pdlc_status {
    // PDLC driver state.
    enum pdlc_state state : 4;
    // Error code of the most recent hiccup or fault (negated to make the constant positive).
    unsigned error : 8;
    // Number of consecutive hiccups.
    unsigned hiccup_count : 4;
    unsigned : 16;
};
_Static_assert(sizeof(struct pdlc_status) == 4, "");

/**
 * @brief Update the PDLC driver state.  Must be called at least once every 10 ms.
 * 
 * @param scale Set to 0 to disable the high voltage supply and turn off the PDLC output.
 * Set to 256 to enable the high voltage supply and output at maximum amplitude or use a
 * value between 0 and 256 to scale the output amplitude proportionally.
 * @param transition Time in milliseconds to ramp the voltage from zero to full scale.
 * @return The status of the driver.
 */
struct pdlc_status pdlc_poll(unsigned scale, unsigned transition);
