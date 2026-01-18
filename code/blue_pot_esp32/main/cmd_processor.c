/*
 * Command Processor for USB/UART Serial Interface
 */

#include "cmd_processor.h"
#include "blue_pot.h"
#include "bt_module.h"
#include "pots_module.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *TAG = "CMD_PROCESSOR";

// Command processor state
static cmd_state_t cmd_state = CMD_ST_IDLE;
static uint8_t cur_cmd = 0;
static int cmd_arg = 0;
static int cmd_val_index = 0;
static uint8_t cmd_val[32];
static int cmd_val_num = 0;
static bool cmd_has_val = false;
static char cmd_buf[32];

// USB/Serial configuration
#define CONSOLE_UART_NUM UART_NUM_0

// NVS handle
static nvs_handle_t nvs_handle;

// =================================================================
// Private Function Declarations
// =================================================================

static bool _validate_command(char c);
static int _is_valid_hex(char c);
static void _process_command(void);
static void _display_usage(void);
static void _uart_event_task(void *pvParameters);

// =================================================================
// Public API
// =================================================================

void cmd_processor_init(void)
{
    ESP_LOGI(TAG, "Initializing command processor");

    cmd_state = CMD_ST_IDLE;

    // Initialize NVS for storing pair ID
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS
    err = nvs_open("blue_pot", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS");
    }

    ESP_LOGI(TAG, "Command processor initialized");
}

void cmd_processor_eval(void)
{
    char c;
    int v;

    while (uart_read_bytes(CONSOLE_UART_NUM, (uint8_t *)&c, 1, 0) > 0) {
        switch (cmd_state) {
            case CMD_ST_IDLE:
                if (_validate_command(c)) {
                    cur_cmd = c;
                    cmd_has_val = false;
                    cmd_arg = 0;
                    cmd_state = CMD_ST_CMD;
                }
                break;

            case CMD_ST_CMD:
                if (c == '\r' || c == '\n') {
                    _process_command();
                    cmd_state = CMD_ST_IDLE;
                } else if ((v = _is_valid_hex(c)) >= 0) {
                    cmd_arg = (cmd_arg * 16) + v;
                } else if (c == '=') {
                    cmd_state = CMD_ST_VAL1;
                    cmd_val_num = 0;
                    cmd_val_index = 0;
                    cmd_val[cmd_val_index] = 0;
                } else {
                    cmd_state = CMD_ST_IDLE;
                    printf("Illegal command\n");
                }
                break;

            case CMD_ST_VAL1:
                if (c == '\r' || c == '\n') {
                    _process_command();
                    cmd_state = CMD_ST_IDLE;
                } else if ((v = _is_valid_hex(c)) >= 0) {
                    cmd_val[cmd_val_index] = v;
                    cmd_val_num++;
                    cmd_has_val = true;
                    cmd_state = CMD_ST_VAL2;
                } else {
                    cmd_state = CMD_ST_IDLE;
                    printf("Illegal command\n");
                }
                break;

            case CMD_ST_VAL2:
                if (c == '\r' || c == '\n') {
                    _process_command();
                    cmd_state = CMD_ST_IDLE;
                } else if (c == ' ') {
                    cmd_val_index++;
                    cmd_val[cmd_val_index] = 0;
                    cmd_state = CMD_ST_VAL1;
                } else if ((v = _is_valid_hex(c)) >= 0) {
                    cmd_val[cmd_val_index] = (cmd_val[cmd_val_index] * 16) + v;
                } else {
                    cmd_state = CMD_ST_IDLE;
                    printf("Illegal command\n");
                }
                break;

            default:
                cmd_state = CMD_ST_IDLE;
        }
    }
}

// =================================================================
// Private Implementation
// =================================================================

static bool _validate_command(char c)
{
    switch (c) {
        case 'D':
        case 'H':
        case 'L':
        case 'P':
        case 'R':
        case 'V':
            return true;
        default:
            return false;
    }
}

static int _is_valid_hex(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return (c - '0');
    } else if ((c >= 'a') && (c <= 'f')) {
        return (c - 'a' + 10);
    } else if ((c >= 'A') && (c <= 'F')) {
        return (c - 'A' + 10);
    }
    return -1;
}

static void _process_command(void)
{
    int pair_id;
    bool result;

    switch (cur_cmd) {
        case 'D':
            if (cmd_has_val) {
                if (cmd_val[0] > 7) {
                    printf("Illegal Device ID\n");
                } else {
                    pair_id = cmd_val[0];
                    nvs_set_i32(nvs_handle, PAIR_ID_KEY, pair_id);
                    nvs_commit(nvs_handle);
                    bt_set_pairing_number(pair_id);
                }
            } else {
                // Get current pair ID
                pair_id = 0;
                nvs_get_i32(nvs_handle, PAIR_ID_KEY, &pair_id);
                printf("Pairing Device ID = %d\n", pair_id);
            }
            break;

        case 'L':
            bt_send_pairing_enable();
            printf("Enable Pairing\n");
            break;

        case 'P':
            if (cmd_has_val) {
                bt_send_generic_packet(cmd_val, cmd_val_num);
            }
            break;

        case 'R':
            bt_reset();
            printf("Reset BM64\n");
            break;

        case 'V':
            if (cmd_has_val) {
                result = (cmd_val[0] != 0);
                bt_set_verbose_logging(result);
                printf("Verbose = %d\n", cmd_val[0]);
            }
            break;

        case 'H':
            _display_usage();
            break;
    }
}

static void _display_usage(void)
{
    printf("\n");
    printf("Command Interface for version %s\n", VERSION);
    printf("   D                : Display the current Bluetooth pairing ID (0-7)\n");
    printf("   D=<N>            : Set the current Bluetooth pairing ID (0-7)\n");
    printf("   L                : Initiate Bluetooth pairing\n");
    printf("   P=[Packet Bytes] : Send packet (hex bytes)\n");
    printf("   R                : Reset BM64\n");
    printf("   V=<N>            : 1: enable / 0: disable verbose mode\n");
    printf("   H                : This help message\n");
    printf("\n");
}
