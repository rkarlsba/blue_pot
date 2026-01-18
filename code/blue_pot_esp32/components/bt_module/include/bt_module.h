/*
 * BM64 Bluetooth Module Interface - ESP32 Native Driver
 * Handles HCI packet communication with BM64 Bluetooth module
 */

#ifndef BT_MODULE_H
#define BT_MODULE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// =================================================================
// Constants
// =================================================================

#define BT_EVAL_MSEC 20
#define NUM_VALID_DIGITS 10
#define BT_RECONNECT_MSEC 60000
#define BT_RX_BUFFER_SIZE 128
#define BT_TX_BUFFER_SIZE 32

// =================================================================
// State Machine Enums
// =================================================================

typedef enum {
    BT_DISCONNECTED,
    BT_CONNECTED_IDLE,
    BT_DIALING,
    BT_CALL_ACTIVE,
    BT_CALL_INITIATED,
    BT_CALL_OUTGOING,
    BT_CALL_RECEIVED
} bt_state_t;

typedef enum {
    CALL_IDLE,
    CALL_VOICEDIAL,
    CALL_INCOMING,
    CALL_OUTGOING,
    CALL_ACTIVE
} bt_call_state_t;

typedef enum {
    BT_RX_IDLE,
    BT_RX_SYNC,
    BT_RX_LEN_H,
    BT_RX_LEN_L,
    BT_RX_CMD,
    BT_RX_DATA,
    BT_RX_CHKSUM
} bt_rx_pkt_state_t;

// =================================================================
// Public API
// =================================================================

/*
 * Initialize Bluetooth module
 * @param device_index - Paired device index (0-7)
 */
void bt_module_init(int device_index);

/*
 * Evaluate Bluetooth module state machine
 * Should be called periodically (at least every 20ms)
 */
void bt_module_eval(void);

/*
 * Deinitialize Bluetooth module
 */
void bt_module_deinit(void);

/*
 * Set pairing number/device index
 * @param n - Device index (0-7)
 */
void bt_set_pairing_number(int n);

/*
 * Enable verbose logging of received packets
 * @param enable - true to enable logging
 */
void bt_set_verbose_logging(bool enable);

/*
 * Send generic BM64 packet
 * @param data - packet data (without sync, length, checksum)
 * @param len - length of data
 */
void bt_send_generic_packet(const uint8_t *data, size_t len);

/*
 * Initiate Bluetooth pairing mode
 */
void bt_send_pairing_enable(void);

/*
 * Reset BM64 module
 */
void bt_reset(void);

// =================================================================
// Status Query Functions
// =================================================================

/*
 * Check if Bluetooth is connected and in service
 */
bool bt_is_in_service(void);

/*
 * Get current Bluetooth connection state
 */
bt_state_t bt_get_state(void);

/*
 * Get current call state
 */
bt_call_state_t bt_get_call_state(void);

/*
 * Check for off-hook event and return status
 * @param off_hook - pointer to bool, set to true if off-hook
 * @return true if state change detected
 */
bool bt_hook_change(bool *off_hook);

/*
 * Check for dialed digit
 * @param digit - pointer to int, receives digit 0-9, 10='*', 11='#'
 * @return true if digit detected
 */
bool bt_digit_dialed(int *digit);

#endif // BT_MODULE_H
