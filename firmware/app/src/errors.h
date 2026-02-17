/**
 * @file
 * @brief Specific error codes that describe the state of the system.
 *
 * These error codes share space with the negative errno set of values and are constrained
 * to fit into 1 byte when reported to external systems so they start at -255 and work
 * their way up.  The color of the blink error is related to the error category and the
 * index determines the number of indicator blinks for that error.
 */

#define ERROR_CATEGORY(err) (-(err) >> 3)
#define ERROR_INDEX(err) ((-(err) & 7) + 1)

// MAGENTA
#define CONTROL_ERROR_FIRST_ (-248)
#define CONTROL_ERROR_INHIBITED (CONTROL_ERROR_FIRST_)  /* Operation of the PDLC output has been remotely inhibited. */

// RED
#define PDLC_ERROR_FIRST_ (-240)
#define PDLC_ERROR_HV_TIMEOUT (PDLC_ERROR_FIRST_)          /* Boost converter did not report power good within the expected time after being enabled. */
#define PDLC_ERROR_HV_FAULT (PDLC_ERROR_FIRST_ - 1)        /* Boost converter reported power good then subsequently reported not power good. */
#define PDLC_ERROR_OUTPUT_TIMEOUT (PDLC_ERROR_FIRST_ - 2)  /* Buck converter did not report power good within the expected time after being enabled. */
#define PDLC_ERROR_OUTPUT_FAULT (PDLC_ERROR_FIRST_ - 3)    /* Buck converter reported power good then subsequently reported not power good. */
#define PDLC_ERROR_DRIVER (PDLC_ERROR_FIRST_ - 4)          /* An error occurred in the driver. */
