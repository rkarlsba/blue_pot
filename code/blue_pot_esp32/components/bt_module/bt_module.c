/*
 * BM64 Bluetooth Module Interface - ESP32 Native Implementation
 * Ported from Arduino-based Teensy code
 */

#include "bt_module.h"
#include "blue_pot.h"
#include "pots_module.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "BT_MODULE";

#define BT_UART_NUM UART_NUM_2
#define BT_UART_BAUD 115200
#define BT_UART_RX_BUF_SIZE 1024

// =================================================================
// Module State Variables
// =================================================================

static bt_state_t bt_state = BT_DISCONNECTED;
static bt_call_state_t bt_call_state = CALL_IDLE;
static bool bt_in_service = false;
static int bt_link_device_index = 0;
static bool bt_verbose_logging = false;
static int bt_reconnect_count = 0;

static const char *bt_state_names[] = {
    "DISCONNECTED",
    "CONNECTED-IDLE",
    "DIALING",
    "ACTIVE",
    "INITIATED",
    "OUTGOING",
    "RECEIVED"
};

static const char *bt_call_state_names[] = {
    "IDLE",
    "VOICEDIAL",
    "INCOMING",
    "OUTGOING",
    "ACTIVE"
};

// RX packet state
static bt_rx_pkt_state_t bt_rx_pkt_state = BT_RX_IDLE;
static uint8_t bt_rx_pkt_buf[32];
static int bt_rx_pkt_index = 0;
static int bt_rx_pkt_len = 0;
static uint8_t bt_rx_pkt_exp_chksum = 0;

// TX packet buffer
static uint8_t bt_tx_pkt_buf[32];

// Dialing state
static int bt_dial_index = 0;
static int bt_dial_array[NUM_VALID_DIGITS];

// Module evaluation timing
static uint64_t bt_prev_eval_time = 0;

// =================================================================
// Private Function Declarations
// =================================================================

static void _bt_init_pins(void);
static void _bt_set_reset_pin(int level);
static void _bt_set_ean_pin(int level);
static void _bt_set_p2_0_pin(int level);
static void _bt_set_mfb_pin(int level);
static void _bt_set_mode(int mode);
static void _bt_do_reset(bool set_mfb);
static bool _bt_eval_timeout(void);
static void _bt_set_state(bt_state_t new_state);
static void _bt_process_rx_data(uint8_t data);
static void _bt_process_rx_pkt(void);
static void _bt_send_event_ack(uint8_t id);
static void _bt_send_link_to_selected_device_index(void);
static void _bt_send_accept_call(void);
static void _bt_send_drop_call(void);
static void _bt_send_dial_number(void);
static void _bt_send_voice_dial(void);
static void _bt_send_set_speaker_gain(uint8_t gain);
static void _bt_send_tx_packet(uint16_t len);
static void _bt_print_hex8(uint8_t n);
static void _bt_print_number(int n);
static void _bt_uart_rx_handler(void);

// =================================================================
// Public API Implementation
// =================================================================

void bt_module_init(int device_index)
{
    ESP_LOGI(TAG, "Initializing BM64 module");

    // Initialize variables
    bt_state = BT_DISCONNECTED;
    bt_call_state = CALL_IDLE;
    bt_in_service = false;
    bt_rx_pkt_state = BT_RX_IDLE;
    bt_rx_pkt_index = 0;
    bt_dial_index = 0;
    bt_verbose_logging = false;
    bt_reconnect_count = BT_RECONNECT_MSEC / BT_EVAL_MSEC;
    bt_link_device_index = device_index & 0x07;

    // Configure UART for BM64
    uart_config_t uart_config = {
        .baud_rate = BT_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    uart_driver_install(BT_UART_NUM, BT_UART_RX_BUF_SIZE, 0, 0, NULL);
    uart_param_config(BT_UART_NUM, &uart_config);
    uart_set_pin(BT_UART_NUM, PIN_BT_TX, PIN_BT_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Initialize hardware
    _bt_init_pins();
    _bt_set_mode(0);  // Flash App mode
    _bt_do_reset(true);

    // Set initial eval time
    bt_prev_eval_time = esp_timer_get_time() / 1000;

    ESP_LOGI(TAG, "BM64 module initialized");
}

void bt_module_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing BM64 module");
    uart_driver_delete(BT_UART_NUM);
}

void bt_module_eval(void)
{
    bool off_hook = false;
    int new_digit = -1;

    // Process any incoming serial data
    _bt_uart_rx_handler();

    if (!_bt_eval_timeout()) {
        return;
    }

    // State machine evaluation
    switch (bt_state) {
        case BT_DISCONNECTED:
            if (bt_in_service) {
                _bt_set_state(BT_CONNECTED_IDLE);
            } else if (--bt_reconnect_count <= 0) {
                ESP_LOGI(TAG, "Connection attempt to device id %d", bt_link_device_index);
                _bt_send_link_to_selected_device_index();
                bt_reconnect_count = BT_RECONNECT_MSEC / BT_EVAL_MSEC;
            }
            break;

        case BT_CONNECTED_IDLE:
            if (!bt_in_service) {
                _bt_set_state(BT_DISCONNECTED);
            } else if (pots_hook_change(&off_hook)) {
                if (off_hook) {
                    _bt_set_state(BT_DIALING);
                }
            } else if (bt_call_state == CALL_INCOMING) {
                _bt_set_state(BT_CALL_RECEIVED);
            }
            break;

        case BT_DIALING:
            if (!bt_in_service) {
                _bt_set_state(BT_DISCONNECTED);
            } else if (pots_digit_dialed(&new_digit)) {
                if ((new_digit == 0) && (bt_dial_index == 0)) {
                    _bt_send_voice_dial();
                    _bt_set_state(BT_CALL_INITIATED);
                    ESP_LOGI(TAG, "Voice Dial");
                } else {
                    bt_dial_array[bt_dial_index++] = new_digit;
                    if (bt_dial_index == NUM_VALID_DIGITS) {
                        _bt_send_dial_number();
                        _bt_set_state(BT_CALL_INITIATED);
                        _bt_print_number(bt_dial_index);
                    }
                }
            } else if (pots_hook_change(&off_hook)) {
                if (!off_hook) {
                    _bt_set_state(BT_CONNECTED_IDLE);
                }
            }
            break;

        case BT_CALL_ACTIVE:
            if (!bt_in_service) {
                _bt_set_state(BT_DISCONNECTED);
            } else if (bt_call_state == CALL_IDLE) {
                _bt_set_state(BT_CONNECTED_IDLE);
            } else if (pots_hook_change(&off_hook)) {
                if (!off_hook) {
                    _bt_send_drop_call();
                    _bt_set_state(BT_CONNECTED_IDLE);
                }
            }
            break;

        case BT_CALL_INITIATED:
            if (!bt_in_service) {
                _bt_set_state(BT_DISCONNECTED);
            } else if (bt_call_state == CALL_ACTIVE) {
                _bt_set_state(BT_CALL_ACTIVE);
            } else if (bt_call_state == CALL_OUTGOING) {
                _bt_set_state(BT_CALL_OUTGOING);
            } else if (pots_hook_change(&off_hook)) {
                if (!off_hook) {
                    _bt_send_drop_call();
                    _bt_set_state(BT_CONNECTED_IDLE);
                }
            }
            break;

        case BT_CALL_OUTGOING:
            if (!bt_in_service) {
                _bt_set_state(BT_DISCONNECTED);
            } else if (bt_call_state == CALL_ACTIVE) {
                _bt_set_state(BT_CALL_ACTIVE);
            } else if (bt_call_state == CALL_IDLE) {
                _bt_set_state(BT_CONNECTED_IDLE);
            } else if (pots_hook_change(&off_hook)) {
                if (!off_hook) {
                    _bt_send_drop_call();
                    _bt_set_state(BT_CONNECTED_IDLE);
                }
            }
            break;

        case BT_CALL_RECEIVED:
            if (!bt_in_service) {
                _bt_set_state(BT_DISCONNECTED);
            } else if (pots_hook_change(&off_hook)) {
                if (off_hook) {
                    _bt_send_accept_call();
                    _bt_set_state(BT_CALL_ACTIVE);
                }
            } else if (bt_call_state != CALL_INCOMING) {
                _bt_set_state(BT_CONNECTED_IDLE);
            }
            break;
    }
}

void bt_set_pairing_number(int n)
{
    bt_link_device_index = n & 0x07;
}

void bt_set_verbose_logging(bool enable)
{
    bt_verbose_logging = enable;
}

void bt_send_pairing_enable(void)
{
    bt_tx_pkt_buf[0] = 0x02;  // MMI Command
    bt_tx_pkt_buf[1] = 0x00;  // Database 0
    bt_tx_pkt_buf[2] = 0x5D;  // Fast enter pairing mode
    _bt_send_tx_packet(3);
}

void bt_send_generic_packet(const uint8_t *data, size_t len)
{
    if (len > sizeof(bt_tx_pkt_buf)) {
        ESP_LOGW(TAG, "Packet too large");
        return;
    }
    memcpy(bt_tx_pkt_buf, data, len);
    _bt_send_tx_packet(len);
}

void bt_reset(void)
{
    _bt_do_reset(true);
}

bool bt_is_in_service(void)
{
    return bt_in_service;
}

bt_state_t bt_get_state(void)
{
    return bt_state;
}

bt_call_state_t bt_get_call_state(void)
{
    return bt_call_state;
}

bool bt_hook_change(bool *off_hook)
{
    // This is a pass-through to the POTS module
    return pots_hook_change(off_hook);
}

bool bt_digit_dialed(int *digit)
{
    // This is a pass-through to the POTS module
    return pots_digit_dialed(digit);
}

// =================================================================
// Private Implementation
// =================================================================

static void _bt_init_pins(void)
{
    gpio_config_t io_conf = {};

    // Configure as output pins
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_BT_RSTN) | (1ULL << PIN_BT_EAN) | 
                           (1ULL << PIN_BT_P2_0) | (1ULL << PIN_BT_MFB);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Set initial values
    gpio_set_level(PIN_BT_RSTN, 1);
    gpio_set_level(PIN_BT_MFB, 0);
}

static void _bt_set_reset_pin(int level)
{
    gpio_set_level(PIN_BT_RSTN, level);
}

static void _bt_set_ean_pin(int level)
{
    if (level == 1) {
        gpio_set_direction(PIN_BT_EAN, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_BT_EAN, 1);
    } else {
        gpio_set_level(PIN_BT_EAN, 0);
        gpio_set_direction(PIN_BT_EAN, GPIO_MODE_INPUT);
    }
}

static void _bt_set_p2_0_pin(int level)
{
    if (level == 1) {
        gpio_set_level(PIN_BT_P2_0, 1);
        gpio_set_direction(PIN_BT_P2_0, GPIO_MODE_INPUT);
    } else {
        gpio_set_direction(PIN_BT_P2_0, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_BT_P2_0, 0);
    }
}

static void _bt_set_mfb_pin(int level)
{
    gpio_set_level(PIN_BT_MFB, level);
}

static void _bt_set_mode(int mode)
{
    // Mode configuration for BM64
    // 0: Flash App (normal operation)
    // 1: Flash IBDK
    // 2: ROM App
    // 3: ROM IBDK
    switch (mode) {
        case 1:  // Flash IBDK
            _bt_set_ean_pin(0);
            _bt_set_p2_0_pin(0);
            break;
        case 2:  // ROM App
            _bt_set_ean_pin(1);
            _bt_set_p2_0_pin(1);
            break;
        case 3:  // ROM IBDK
            _bt_set_ean_pin(1);
            _bt_set_p2_0_pin(0);
            break;
        default:  // Flash App
            _bt_set_ean_pin(0);
            _bt_set_p2_0_pin(1);
    }
}

static void _bt_do_reset(bool set_mfb)
{
    if (set_mfb) {
        _bt_set_mfb_pin(0);
    }

    _bt_set_reset_pin(0);
    vTaskDelay(pdMS_TO_TICKS(499));

    if (set_mfb) {
        _bt_set_mfb_pin(1);
    }

    vTaskDelay(pdMS_TO_TICKS(1));
    _bt_set_reset_pin(1);
}

static bool _bt_eval_timeout(void)
{
    uint64_t current_time = esp_timer_get_time() / 1000;
    uint64_t delta_time;

    if (current_time >= bt_prev_eval_time) {
        delta_time = current_time - bt_prev_eval_time;
    } else {
        // Handle wrap
        delta_time = ~(bt_prev_eval_time - current_time) + 1;
    }

    if (delta_time >= BT_EVAL_MSEC) {
        bt_prev_eval_time = current_time;
        return true;
    }

    return false;
}

static void _bt_set_state(bt_state_t new_state)
{
    switch (new_state) {
        case BT_DISCONNECTED:
            pots_set_in_service(false);
            pots_set_in_call(false);
            pots_set_ring(false);
            bt_reconnect_count = BT_RECONNECT_MSEC / BT_EVAL_MSEC;
            break;

        case BT_CONNECTED_IDLE:
            pots_set_in_service(true);
            pots_set_in_call(false);
            pots_set_ring(false);
            break;

        case BT_DIALING:
            bt_dial_index = 0;
            break;

        case BT_CALL_ACTIVE:
            _bt_send_set_speaker_gain(0x0E);
            pots_set_in_call(true);
            pots_set_ring(false);
            break;

        case BT_CALL_INITIATED:
            break;

        case BT_CALL_OUTGOING:
            break;

        case BT_CALL_RECEIVED:
            pots_set_ring(true);
            break;
    }

    ESP_LOGI(TAG, "BT State: %s -> %s", bt_state_names[bt_state], bt_state_names[new_state]);
    bt_state = new_state;
}

static void _bt_process_rx_data(uint8_t data)
{
    switch (bt_rx_pkt_state) {
        case BT_RX_IDLE:
            if (data == 0x00) {
                bt_rx_pkt_state = BT_RX_SYNC;
                bt_rx_pkt_index = 0;
                bt_rx_pkt_buf[bt_rx_pkt_index++] = data;
            }
            break;

        case BT_RX_SYNC:
            if (data == 0xAA) {
                bt_rx_pkt_state = BT_RX_LEN_H;
                bt_rx_pkt_buf[bt_rx_pkt_index++] = data;
            } else {
                bt_rx_pkt_state = BT_RX_IDLE;
            }
            break;

        case BT_RX_LEN_H:
            bt_rx_pkt_state = BT_RX_LEN_L;
            bt_rx_pkt_len = data * 0x100;
            bt_rx_pkt_exp_chksum = data;
            bt_rx_pkt_buf[bt_rx_pkt_index++] = data;
            break;

        case BT_RX_LEN_L:
            bt_rx_pkt_state = BT_RX_CMD;
            bt_rx_pkt_len += data;
            bt_rx_pkt_exp_chksum += data;
            bt_rx_pkt_buf[bt_rx_pkt_index++] = data;
            break;

        case BT_RX_CMD:
            bt_rx_pkt_state = BT_RX_DATA;
            bt_rx_pkt_exp_chksum += data;
            bt_rx_pkt_buf[bt_rx_pkt_index++] = data;
            break;

        case BT_RX_DATA:
            if (bt_rx_pkt_index == (bt_rx_pkt_len + 3)) {
                bt_rx_pkt_state = BT_RX_CHKSUM;
            }
            bt_rx_pkt_exp_chksum += data;
            bt_rx_pkt_buf[bt_rx_pkt_index++] = data;
            break;

        case BT_RX_CHKSUM:
            bt_rx_pkt_state = BT_RX_IDLE;
            bt_rx_pkt_exp_chksum = ~bt_rx_pkt_exp_chksum + 1;

            if (bt_verbose_logging) {
                if (bt_rx_pkt_exp_chksum != data) {
                    printf("BAD ");
                }
                printf("RX: ");
                for (int i = 0; i < (bt_rx_pkt_len + 5); i++) {
                    _bt_print_hex8(bt_rx_pkt_buf[i]);
                }
                printf("\n");
            }

            if ((bt_rx_pkt_buf[4] != 0x00) && (bt_rx_pkt_exp_chksum == data)) {
                _bt_send_event_ack(bt_rx_pkt_buf[4]);
                _bt_process_rx_pkt();
            }
            break;

        default:
            bt_rx_pkt_state = BT_RX_IDLE;
    }
}

static void _bt_process_rx_pkt(void)
{
    switch (bt_rx_pkt_buf[4]) {
        case 0x01:  // BTM_Status
            if (bt_rx_pkt_buf[5] == 0x05) {
                bt_in_service = true;
                ESP_LOGI(TAG, "HF/HS Link established");
            } else if (bt_rx_pkt_buf[5] == 0x07) {
                bt_in_service = false;
                ESP_LOGI(TAG, "HF Link disconnected");
            }
            break;

        case 0x02:  // Call_Status
            switch (bt_rx_pkt_buf[6]) {
                case 0x00:
                    bt_call_state = CALL_IDLE;
                    break;
                case 0x01:
                    bt_call_state = CALL_VOICEDIAL;
                    break;
                case 0x02:
                    bt_call_state = CALL_INCOMING;
                    break;
                case 0x03:
                    bt_call_state = CALL_OUTGOING;
                    break;
                case 0x04:
                    bt_call_state = CALL_ACTIVE;
                    break;
            }
            ESP_LOGI(TAG, "Call: %s", bt_call_state_names[bt_call_state]);
            break;

        case 0x03:  // Caller_ID
            printf("Caller ID: ");
            for (int i = 0; i < (bt_rx_pkt_buf[3] - 2); i++) {
                printf("%c", bt_rx_pkt_buf[i + 6]);
            }
            printf("\n");
            break;
    }
}

static void _bt_send_event_ack(uint8_t id)
{
    bt_tx_pkt_buf[0] = 0x14;
    bt_tx_pkt_buf[1] = id;
    _bt_send_tx_packet(2);
}

static void _bt_send_link_to_selected_device_index(void)
{
    bt_tx_pkt_buf[0] = 0x17;
    bt_tx_pkt_buf[1] = 0x04;
    bt_tx_pkt_buf[2] = (uint8_t)bt_link_device_index;
    bt_tx_pkt_buf[3] = 0x03;
    _bt_send_tx_packet(4);
}

static void _bt_send_accept_call(void)
{
    bt_tx_pkt_buf[0] = 0x02;
    bt_tx_pkt_buf[1] = 0x00;
    bt_tx_pkt_buf[2] = 0x04;
    _bt_send_tx_packet(3);
}

static void _bt_send_drop_call(void)
{
    bt_tx_pkt_buf[0] = 0x02;
    bt_tx_pkt_buf[1] = 0x00;
    bt_tx_pkt_buf[2] = 0x06;
    _bt_send_tx_packet(3);
}

static void _bt_send_dial_number(void)
{
    bt_tx_pkt_buf[0] = 0x00;
    bt_tx_pkt_buf[1] = 0x00;

    for (int i = 0; i < NUM_VALID_DIGITS; i++) {
        if (bt_dial_array[i] == 10) {
            bt_tx_pkt_buf[i + 2] = '*';
        } else if (bt_dial_array[i] == 11) {
            bt_tx_pkt_buf[i + 2] = '#';
        } else {
            bt_tx_pkt_buf[i + 2] = '0' + bt_dial_array[i];
        }
    }
    _bt_send_tx_packet(12);
}

static void _bt_send_voice_dial(void)
{
    bt_tx_pkt_buf[0] = 0x02;
    bt_tx_pkt_buf[1] = 0x00;
    bt_tx_pkt_buf[2] = 0x0A;
    _bt_send_tx_packet(3);
}

static void _bt_send_set_speaker_gain(uint8_t gain)
{
    bt_tx_pkt_buf[0] = 0x1B;
    bt_tx_pkt_buf[1] = 0x00;
    bt_tx_pkt_buf[2] = gain & 0x0F;
    _bt_send_tx_packet(3);
}

static void _bt_send_tx_packet(uint16_t len)
{
    uint8_t checksum = (len >> 8);

    uart_write_bytes(BT_UART_NUM, (const char *)"\x00\xAA", 2);
    uart_write_bytes(BT_UART_NUM, (const char *)&((len >> 8) & 0xFF), 1);
    uart_write_bytes(BT_UART_NUM, (const char *)&(len & 0xFF), 1);

    checksum += (len & 0xFF);

    if (bt_verbose_logging) {
        printf("TX: 00 AA ");
        _bt_print_hex8(len >> 8);
        _bt_print_hex8(len & 0xFF);
    }

    for (uint16_t i = 0; i < len; i++) {
        uart_write_bytes(BT_UART_NUM, (const char *)&bt_tx_pkt_buf[i], 1);
        checksum += bt_tx_pkt_buf[i];

        if (bt_verbose_logging) {
            _bt_print_hex8(bt_tx_pkt_buf[i]);
        }
    }

    uint8_t final_checksum = ~checksum + 1;
    uart_write_bytes(BT_UART_NUM, (const char *)&final_checksum, 1);

    if (bt_verbose_logging) {
        _bt_print_hex8(final_checksum);
        printf("\n");
    }
}

static void _bt_print_hex8(uint8_t n)
{
    printf("%02X ", n);
}

static void _bt_print_number(int n)
{
    printf("Number: ");
    for (int i = 0; i < n; i++) {
        if (bt_dial_array[i] == 10) {
            printf("*");
        } else if (bt_dial_array[i] == 11) {
            printf("#");
        } else {
            printf("%d", bt_dial_array[i]);
        }
    }
    printf("\n");
}

static void _bt_uart_rx_handler(void)
{
    uint8_t data;
    int len = 0;

    while (uart_read_bytes(BT_UART_NUM, &data, 1, 0) > 0) {
        _bt_process_rx_data(data);
        len++;
    }
}
