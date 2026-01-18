/*
 * Blue POT for ESP32 - Bluetooth to POTS Gateway
 * Main header file
 */

#ifndef BLUE_POT_H
#define BLUE_POT_H

#include <stdint.h>
#include <stdbool.h>

#define VERSION "2.0-ESP32"

// =================================================================
// ESP32 Pin Assignments (UART2 for Bluetooth module)
// =================================================================
#define PIN_BT_RX    16  // GPIO16 - BM64 TX
#define PIN_BT_TX    17  // GPIO17 - BM64 RX

#define PIN_BT_RSTN  18  // GPIO18 - BM64 Reset (active low)
#define PIN_BT_EAN   19  // GPIO19 - BM64 EAN configuration
#define PIN_BT_P2_0  21  // GPIO21 - BM64 P2_0 configuration
#define PIN_BT_MFB   22  // GPIO22 - BM64 Multi-Function Button

// POTS Control Pins
#define PIN_POTS_FR  32  // GPIO32 - AG1171 FR (Tip/Ring Forward/nReverse)
#define PIN_POTS_RM  33  // GPIO33 - AG1171 RM (Ring Mode control)
#define PIN_POTS_SHK 34  // GPIO34 - AG1171 SHK (Switch Hook - input only)
#define PIN_POTS_LED 25  // GPIO25 - Off-hook LED indicator

// ADC Pins (for DTMF detection and AG1171 VOUT)
#define PIN_ADC_VOUT 35  // GPIO35 (ADC1_CH7) - AG1171 VOUT

// DAC Pins (for AG1171 VIN tone generation)
#define PIN_DAC_VIN  25  // GPIO25 (DAC2) - AG1171 VIN

// Configuration Pin (not used on ESP32 - could be jumper input)
// #define PIN_FUNC  23  // Jumper input (optional)

// =================================================================
// Command Processor States
// =================================================================
typedef enum {
    CMD_ST_IDLE = 0,
    CMD_ST_CMD = 1,
    CMD_ST_VAL1 = 2,
    CMD_ST_VAL2 = 3
} cmd_state_t;

// =================================================================
// EEPROM/NVS Settings
// =================================================================
#define PAIR_ID_KEY "pair_id"

#endif // BLUE_POT_H
