/*
 * AG1171 SLIC (POTS) Module Interface - ESP32 Native Driver
 * Handles telephone interface, ringing, DTMF detection, and tone generation
 */

#ifndef POTS_MODULE_H
#define POTS_MODULE_H

#include <stdint.h>
#include <stdbool.h>

// =================================================================
// Constants
// =================================================================

#define POTS_EVAL_MSEC 10
#define POTS_ON_HOOK_DETECT_MSEC 500
#define POTS_DTMF_DRIVEN_MSEC 30
#define POTS_DTMF_SILENT_MSEC 30

#define POTS_RING_ON_MSEC 1000
#define POTS_RING_OFF_MSEC 3000
#define POTS_RING_FREQ_HZ 25

#define POTS_ROT_BREAK_MSEC 100
#define POTS_ROT_MAKE_MSEC 100

#define POTS_NS_TONE_ON_MSEC 300
#define POTS_NS_TONE_OFF_MSEC 200

#define POTS_OH_TONE_ON_MSEC 100
#define POTS_OH_TONE_OFF_MSEC 100

#define POTS_RCV_OFF_HOOK_MSEC 60000

#define POTS_DTMF_ROW_THRESHOLD 0.2
#define POTS_DTMF_COL_THRESHOLD 0.2

#define POTS_DTMF_ASTERISK_VAL 10
#define POTS_DTMF_POUND_VAL 11

// =================================================================
// State Machine Enums
// =================================================================

typedef enum {
    POTS_ON_HOOK,
    POTS_OFF_HOOK,
    POTS_ON_HOOK_PROVISIONAL,
    POTS_RINGING
} pots_state_t;

typedef enum {
    POTS_RING_IDLE,
    POTS_RING_PULSE_ON,
    POTS_RING_PULSE_OFF,
    POTS_RING_BETWEEN
} pots_ring_state_t;

typedef enum {
    POTS_DIAL_IDLE,
    POTS_DIAL_BREAK,
    POTS_DIAL_MAKE,
    POTS_DIAL_DTMF_ON,
    POTS_DIAL_DTMF_OFF
} pots_dial_state_t;

typedef enum {
    POTS_TONE_IDLE,
    POTS_TONE_OFF,
    POTS_TONE_DIAL,
    POTS_TONE_NO_SERVICE_ON,
    POTS_TONE_NO_SERVICE_OFF,
    POTS_TONE_OFF_HOOK_ON,
    POTS_TONE_OFF_HOOK_OFF
} pots_tone_state_t;

// =================================================================
// Public API
// =================================================================

/*
 * Initialize POTS module
 */
void pots_module_init(void);

/*
 * Evaluate POTS module state machine
 * Should be called periodically (at least every 10ms)
 */
void pots_module_eval(void);

/*
 * Deinitialize POTS module
 */
void pots_module_deinit(void);

/*
 * Set service availability (enables dial tone when off-hook)
 * @param enable - true to enable service, false to disable
 */
void pots_set_in_service(bool enable);

/*
 * Initiate or end ringing
 * @param enable - true to start ringing, false to stop
 */
void pots_set_ring(bool enable);

/*
 * Set call status
 * @param in_call - true when call is active, false otherwise
 */
void pots_set_in_call(bool in_call);

/*
 * Check for hook state change
 * @param off_hook - pointer to bool, set true if now off-hook, false if on-hook
 * @return true if state change detected
 */
bool pots_hook_change(bool *off_hook);

/*
 * Check for dialed digit
 * @param digit - pointer to int, receives digit 0-9, 10='*', 11='#'
 * @return true if digit detected
 */
bool pots_digit_dialed(int *digit);

// =================================================================
// Status Query Functions
// =================================================================

/*
 * Get current hook state
 */
pots_state_t pots_get_state(void);

/*
 * Check if phone is off-hook
 */
bool pots_is_off_hook(void);

#endif // POTS_MODULE_H
