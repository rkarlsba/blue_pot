# Blue POT for ESP32

ESP-IDF native port of the Blue POT Bluetooth-to-POTS telephone gateway, originally designed for Teensy 3.2 with Arduino.

## Overview

This project implements a gateway between Bluetooth (via BM64 module) and traditional POTS (Plain Old Telephone Service) telephone lines (via AG1171 SLIC).

### Key Features

- **Bluetooth Interface**: BM64 module connection via UART for HFP (Hands-Free Profile)
- **POTS Interface**: AG1171 SLIC telephone line interface
- **Audio**: Dial tone, busy tone, and phone ringing generation
- **Dialing**: Both DTMF and rotary dial support
- **Call Management**: Handle incoming/outgoing calls, call state tracking
- **USB/Serial Console**: Command interface for configuration and debugging
- **Configuration**: Persistent pairing device selection via NVS flash

## Hardware Requirements

### ESP32-WROOM Module
- Development board or standalone module
- 3.3V power supply
- USB-to-Serial adapter for console (optional, can use onboard UART)

### BM64 Bluetooth Module
- Connected to ESP32 via UART2:
  - GPIO16 (RX) ← BM64 TX
  - GPIO17 (TX) → BM64 RX
  - GPIO18 → BM64 RSTN (Reset)
  - GPIO19 → BM64 EAN (Configuration)
  - GPIO21 → BM64 P2_0 (Configuration)
  - GPIO22 → BM64 MFB (Multi-Function Button)

### AG1171 SLIC Telephone Interface
- Connected to ESP32:
  - GPIO32 ↔ AG1171 FR (Tip/Ring control)
  - GPIO33 ↔ AG1171 RM (Ring Mode)
  - GPIO34 (input) ← AG1171 SHK (Switch Hook detection)
  - GPIO25 → DAC2 (Tone output to AG1171 VIN)
  - GPIO35 (ADC1_CH7) ← AG1171 VOUT (Tone input for DTMF detection)
  - GPIO25 (LED output, can be changed)

### Audio Connections
- DAC2 output → 1µF capacitor → AG1171 VIN
- AG1171 VOUT → 1µF capacitor + bias circuit → ADC1_CH7
- BM64 AOHPR → 1µF capacitor → AG1171 VIN (audio pass-through)
- BM64 MIC_P1 → 0.1µF capacitor + voltage divider → AG1171 VOUT

## Pin Configuration Reference

```
ESP32 Pin   | Function           | BM64/AG1171 Pin
────────────┼────────────────────┼─────────────────
GPIO16      | UART2 RX           | BM64 TX
GPIO17      | UART2 TX           | BM64 RX
GPIO18      | Output             | BM64 RSTN
GPIO19      | Output/Input       | BM64 EAN
GPIO21      | Output/Input       | BM64 P2_0
GPIO22      | Output             | BM64 MFB
GPIO32      | Output             | AG1171 FR
GPIO33      | Output             | AG1171 RM
GPIO34      | Input (ADC safe)   | AG1171 SHK
GPIO25      | DAC2/Output        | AG1171 VIN / LED
GPIO35      | ADC1_CH7 (input)   | AG1171 VOUT
GPIO1       | UART0 TX           | Console USB-Serial
GPIO3       | UART0 RX           | Console USB-Serial
```

## Building and Flashing

### Prerequisites
- ESP-IDF 4.4 or later
- esp-idf tools installed
- USB cable connected to ESP32

### Build
```bash
cd blue_pot_esp32
idf.py build
```

### Flash
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Replace `/dev/ttyUSB0` with your serial port (COM3, /dev/ttyACM0, etc.)

### Monitor Serial Output
```bash
idf.py -p /dev/ttyUSB0 monitor
```

## Software Architecture

### Components

1. **Main Application** (`main/main.c`)
   - FreeRTOS task orchestration
   - System initialization
   - Three independent evaluation tasks running at different rates

2. **Bluetooth Module** (`components/bt_module/`)
   - BM64 HCI packet protocol implementation
   - State machine for connection, dialing, and call management
   - Bluetooth service level control

3. **POTS Module** (`components/pots_module/`)
   - AG1171 SLIC interface
   - Hook switch detection and debouncing
   - Ring pulse generation
   - DTMF/rotary dialing detection
   - Tone generation (dial tone, busy tone, receiver off-hook tone)
   - Ringing state machine

4. **Command Processor** (`main/cmd_processor.c`)
   - Serial command interface
   - Configuration persistence (NVS)
   - Device pairing selection
   - Debug packet sending

### Task Scheduling

- **POTS Evaluation Task** (Core 0, Priority +2)
  - Runs every 10ms
  - Handles hook switch, ringing, DTMF detection, tone generation
  - Most time-critical operations

- **Bluetooth Evaluation Task** (Core 1, Priority +1)
  - Runs every 20ms
  - Manages BM64 communication and state machine
  - Cross-module coordination with POTS

- **Command Processor Task** (Core 0, Default Priority)
  - Runs every 10ms
  - Handles serial input/output
  - Non-blocking command processing

## Command Interface

Access via USB serial console at 115200 baud.

### Commands

| Command | Function |
|---------|----------|
| `D` | Display current Bluetooth pairing ID (0-7) |
| `D=<N>` | Set Bluetooth pairing ID (0-7) |
| `L` | Initiate Bluetooth pairing mode |
| `P=[hex...]` | Send raw BM64 packet (for debugging) |
| `R` | Reset BM64 module |
| `V=<0\|1>` | Disable/enable verbose packet logging |
| `H` | Display help |

### Example Usage

```
# Connect and pair with device 2
D=2
L

# Display current settings
D

# Reset Bluetooth module
R

# Enable verbose logging
V=1
```

## State Machines

### Bluetooth Connection State
```
DISCONNECTED
  ↓
CONNECTED_IDLE ← ← → CALL_RECEIVED
  ↓                     ↓
DIALING         (wait for off-hook)
  ↓                     ↓
CALL_INITIATED   CALL_ACTIVE
  ↓              ↑
CALL_OUTGOING → ┘
```

### Phone Hook State
```
ON_HOOK
  ↓
OFF_HOOK → ON_HOOK_PROVISIONAL
  ↑              ↓
  └──────────────┘
     (RINGING)
```

### DTMF/Rotary Dial Detection
- Rotary: Pulse counting with break/make timing
- DTMF: Tone duration and inter-digit timing validation

### Tone Generation States
- `TONE_DIAL`: 350Hz + 440Hz (dial tone)
- `TONE_NO_SERVICE`: 480Hz + 620Hz (busy/no service)
- `TONE_OFF_HOOK`: 1400Hz + 2060Hz + 2450Hz + 2600Hz (receiver off-hook warning)

## Known Limitations and TODO Items

### Current Implementation
- ✅ Bluetooth module control and state machine
- ✅ Phone hook switch detection
- ✅ Ring pulse generation
- ✅ Basic command interface
- ⚠️ DTMF detection (simplified - needs Goertzel algorithm or FFT)
- ⚠️ Tone generation (DAC output infrastructure needs optimization)
- ⚠️ Audio path (uses GPIO DAC, may need I2S for better quality)

### Recommendations for Production

1. **Audio Processing**:
   - Implement proper Goertzel algorithm for DTMF detection
   - Consider using ESP-ADF (Audio Development Framework) for I2S
   - Implement DMA-based continuous DAC output

2. **Robustness**:
   - Add watchdog timer
   - Implement BM64 auto-recovery on communication failure
   - Add electrical isolation/protection for POTS interface

3. **Testing**:
   - Test with real BM64 and AG1171 modules
   - Validate audio quality and DTMF detection accuracy
   - Test all state transition combinations

4. **Optional Features**:
   - Add web UI for configuration
   - Implement call logging/history
   - Add support for different phone dial modes

## Differences from Original Teensy Code

1. **Platform**: ESP32 (FreeRTOS + IDF) vs Teensy 3.2 (Arduino)
2. **Audio Library**: No equivalent to Arduino Audio Library
   - DTMF detection implemented without library (needs improvement)
   - Tone generation uses GPIO DAC instead of Audio Shield
   - Consider I2S with external codec for production

3. **Pin Mapping**: Updated to ESP32 GPIO numbers
4. **UART**: Uses ESP-IDF UART driver instead of Arduino Serial
5. **Storage**: Uses NVS instead of EEPROM
6. **Task Scheduling**: Uses FreeRTOS tasks instead of Arduino loop
7. **Timing**: Uses esp_timer instead of millis()

## Configuration

Edit [sdkconfig.defaults](sdkconfig.defaults) or use `idf.py menuconfig` to adjust:
- Partition layout
- Flash size
- Debug options
- Component options

## License

This is a port based on the original Blue POT code by Dan Julio.
See original project for licensing information.

## References

- [BM64 Bluetooth Module Datasheet](https://www.microchip.com)
- [AG1171 SLIC Datasheet](https://www.microsemi.com)
- [ESP32 Technical Reference Manual](https://www.espressif.com)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf)
