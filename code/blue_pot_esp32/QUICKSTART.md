# Quick Reference Guide

## Getting Started (5 minutes)

### 1. Install Arduino IDE
- Download from arduino.cc
- Install ESP32 board package (Tools → Board Manager → ESP32)

### 2. Connect Hardware
```
ESP32 Pin 2  → AG1171 FR
ESP32 Pin 4  → AG1171 RM  
ESP32 Pin 5  ← AG1171 SHK
ESP32 DAC    → AG1171 VIN (via 1µF cap)
AG1171 VOUT  → ESP32 ADC (via 1µF cap)
```

### 3. Upload Code
- Open `blue_pot_esp32.ino` in Arduino IDE
- Select Tools → Board → ESP32 Dev Module
- Click Upload

### 4. Test
- Open Serial Monitor (115200 baud)
- Type `H` and press Enter
- You should see help message

## Serial Commands

| Command | Example | Purpose |
|---------|---------|---------|
| `D` | `D<CR>` | Show pairing device ID |
| `D=<N>` | `D=3<CR>` | Set pairing device to 3 |
| `L` | `L<CR>` | Enable Bluetooth pairing |
| `V=<N>` | `V=1<CR>` | Enable verbose logging |
| `H` | `H<CR>` | Show this help |

**Note**: Arguments are hexadecimal. Commands end with CR (Enter key).

## GPIO Pin Map

```
GPIO2   = AG1171 FR (Ring Forward/Reverse)
GPIO4   = AG1171 RM (Ring Mode)
GPIO5   = AG1171 SHK (Switch Hook input)
GPIO25  = DAC (tone output) & LED
GPIO35  = ADC (line input)
```

Change in `config.h` if needed.

## Bluetooth Pairing

1. Type `L` in serial console
2. Look for "BluePot" on phone
3. Pair (PIN usually 0000)
4. Device is now connected

## Troubleshooting

### "No serial port"
- Check USB cable is connected
- Install USB-to-UART driver (CP2102)
- Verify port in Arduino IDE (Tools → Port)

### "Upload fails"
- Hold BOOT button while uploading
- Check port speed: 921600 for upload, 115200 for serial

### "Bluetooth won't pair"
- Type `V=1` to enable verbose logging
- Check serial monitor for BT state messages
- Restart Bluetooth on phone

### "Hook detection not working"
- Check GPIO5 is connected to AG1171 SHK
- Type `V=1` to see GPIO state changes
- Verify AG1171 power supply (check with multimeter)

### "No tone heard"
- Current version uses simplified DAC output
- For full multi-frequency tones, see README.md
- Check audio connections to AG1171

## File Overview

| File | Purpose |
|------|---------|
| `blue_pot_esp32.ino` | Main program & FreeRTOS setup |
| `config.h` | All pin definitions & constants |
| `bluetooth_classic.cpp` | Bluetooth implementation |
| `pots_interface.cpp` | Phone line interface |
| `command_processor.cpp` | Serial command processing |
| `platformio.ini` | Build configuration |
| `README.md` | Full documentation |

## Common Changes

### Change Serial Baud Rate
Edit `config.h`:
```cpp
#define UART_BAUD   115200  // Change this
```

### Change LED Pin
Edit `config.h`:
```cpp
#define PIN_STATUS_LED 25   // Change to any GPIO
```

### Adjust Timing
Edit `config.h`:
```cpp
#define BT_EVAL_MSEC    20    // Bluetooth check frequency
#define POTS_EVAL_MSEC  10    // Phone line check frequency
```

### Enable/Disable Debug Output
Edit `config.h`:
```cpp
#define DEBUG_ENABLED 1   // Set to 0 to disable
```

## Memory Usage

```
Code:        ~320KB (FreeRTOS + Bluetooth library)
Static Data: ~2KB (buffers, arrays, state)
Stack:       ~12KB (3 FreeRTOS tasks)
Free RAM:    ~300KB (available for growth)
```

All allocations are static - no malloc/new used.

## Task Information

```
BT_Task:   Bluetooth state machine
           Runs every 20ms
           Priority: 2 (higher)
           Core: 1 (Pro core)

POTS_Task: Phone line interface
           Runs every 10ms  
           Priority: 2 (higher)
           Core: 1 (Pro core)

CMD_Task:  Serial command processor
           Runs every 10ms
           Priority: 1 (lower)
           Core: 0 (App core)
```

## State Machine States

### Bluetooth States
- DISCONNECTED: Looking for pairing
- CONNECTED_IDLE: Ready to make/receive calls
- CALL_INCOMING: Receiving a call (ringing)
- CALL_OUTGOING: Making a call
- CALL_ACTIVE: Call in progress

### Phone States
- ON_HOOK: Handset is hung up
- OFF_HOOK: Handset is picked up
- ON_HOOK_PROVISIONAL: Debouncing hangup
- RINGING: Incoming ring signal active

## Building with PlatformIO

```bash
# One-time setup
pip install platformio

# Build
platformio run

# Upload
platformio run -t upload

# Monitor serial output
platformio device monitor -b 115200

# Clean build
platformio run -t clean
```

## Frequency Reference

| Function | Freq 1 | Freq 2 | Freq 3 | Freq 4 |
|----------|--------|--------|--------|--------|
| Dial Tone | 350Hz | 440Hz | - | - |
| No Service | 480Hz | 620Hz | - | - |
| Off-hook | 1400Hz | 2060Hz | 2450Hz | 2600Hz |

## Hardware Pins Reference

```
Power:
  GND - Ground
  3V3 - 3.3V output (max 500mA)
  VIN - 5-12V input

UART (serial debug):
  U0_RXD - GPIO3
  U0_TXD - GPIO1

ADC (analog input):
  GPIO32-39 - 8-channel 12-bit ADC

DAC (analog output):
  GPIO25 - DAC1 (8-bit, 0-3.3V)
  GPIO26 - DAC2 (8-bit, 0-3.3V)

SPI:
  MOSI - GPIO23
  MISO - GPIO19
  SCK  - GPIO18
  CS   - GPIO5/GPIO21

Bluetooth/WiFi:
  Antenna - Built-in

Status:
  GPIO2 - Can use for LED (built-in LED on many boards)
```

## Power Requirements

```
ESP32 idle:     ~10mA @ 3.3V
ESP32 + WiFi:   ~80mA @ 3.3V
ESP32 + BT:     ~80mA @ 3.3V
AG1171:         ~50mA @ 5V

Total minimum:  ~130mA

Recommended PSU: 500mA @ 5V
```

## Next: Customization

After basic operation:

1. **Add persistent storage** → Use NVS instead of EEPROM
2. **Improve audio** → DMA + timer for waveforms  
3. **Add DTMF** → Signal processing library
4. **Web interface** → WiFi + web server
5. **Ring detection** → AC voltage sensing circuit

## Support

- Check `README.md` for detailed troubleshooting
- Check `IMPLEMENTATION.md` for architecture details
- Enable verbose mode: Type `V=1`
- Check GPIO connections with multimeter

---

**Version**: 1.0  
**Date**: January 2026  
**Status**: Ready for deployment
