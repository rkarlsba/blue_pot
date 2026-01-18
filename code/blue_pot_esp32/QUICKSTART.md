# Quick Start Guide for Blue POT ESP32

## 1. Prerequisites

### Software
- ESP-IDF 4.4 or later (recommended 5.0+)
- Python 3.7+
- Git

### Hardware
- ESP32-WROOM development board
- BM64 Bluetooth module
- AG1171 SLIC telephone interface module
- USB cable for programming
- 3.3V power supply

## 2. Installation

### Step 1: Set up ESP-IDF
```bash
# Clone ESP-IDF repository
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git -b v5.0
cd esp-idf
./install.sh
source export.sh
```

### Step 2: Clone and Build Blue POT
```bash
cd ~/projects  # or your preferred location
git clone https://github.com/yourusername/blue_pot.git
cd blue_pot/code/blue_pot_esp32

# On Linux/macOS:
bash build.sh /dev/ttyUSB0 build

# On Windows (using Git Bash):
bash build.sh COM3 build
```

## 3. Wiring the Hardware

### BM64 Bluetooth Module
Connect to ESP32:
```
BM64 TX     → ESP32 GPIO16
BM64 RX     ← ESP32 GPIO17
BM64 RSTN   ← ESP32 GPIO18
BM64 VCC    → 3.3V (via 1µF cap)
BM64 GND    → GND
BM64 MFB    ← ESP32 GPIO22 (hold LOW on boot for normal mode)
BM64 EAN    ← ESP32 GPIO19 (LOW on boot for Flash App mode)
BM64 P2_0   ← ESP32 GPIO21 (HIGH on boot for Flash App mode)
```

### AG1171 SLIC Telephone Interface
Connect to ESP32:
```
AG1171 FR   ← ESP32 GPIO32
AG1171 RM   ← ESP32 GPIO33
AG1171 SHK  → ESP32 GPIO34
AG1171 VIN  ← ESP32 GPIO25 (via 1µF cap)
AG1171 VOUT → ESP32 GPIO35 (via 1µF cap + bias circuit)
AG1171 VCC  → 3.3V (separate supply if possible)
AG1171 GND  → GND
```

### Phone Connection
```
AG1171 TIP  → Phone Line Tip
AG1171 RING → Phone Line Ring
```

See [PIN_CONFIG.md](PIN_CONFIG.md) for detailed pinout information.

## 4. Build and Flash

### Quick Build
```bash
# Using the build script (Linux/macOS):
./build.sh /dev/ttyUSB0 build-flash-monitor

# Manual build with ESP-IDF:
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor
```

### Troubleshooting Build Issues
```bash
# Clean build directory
idf.py fullclean

# Check ESP-IDF version
idf.py --version

# Rebuild with verbose output
idf.py -v build
```

## 5. First Run

### What to Expect
1. ESP32 boots and initializes all modules
2. Serial console shows startup messages (115200 baud)
3. BM64 attempts to reconnect to last paired device
4. POTS module ready for phone use

### Serial Console Access
```bash
# Linux/macOS
screen /dev/ttyUSB0 115200
miniterm.py /dev/ttyUSB0 115200  # Alternative

# Windows (using Putty or similar)
COM3 115200 baud

# Or use IDF monitor:
idf.py -p /dev/ttyUSB0 monitor
```

### Initial Commands
```
H                    # Show help menu
D                    # Show current pairing device ID
D=0                  # Set pairing device to 0
L                    # Enable Bluetooth pairing mode
V=1                  # Enable verbose packet logging
```

## 6. Configuration

### Persistent Configuration
Settings are stored in NVS (Non-Volatile Storage):
```bash
# Access via serial console:
D=2              # Set pairing ID to device 2 (persists after reboot)
D                # Verify current setting
```

### Customizing Pin Assignments
Edit [main/include/blue_pot.h](main/include/blue_pot.h):
```c
#define PIN_BT_RX    16  // Change GPIO numbers here
#define PIN_BT_TX    17
// ... etc
```

Then rebuild:
```bash
idf.py build
```

## 7. Testing the System

### Bluetooth Testing
1. Power up BM64 and ESP32
2. Send `L` command (enter pairing mode)
3. Pair smartphone with BM64
4. Check `D` - should show paired device
5. Call and verify audio routing

### Phone Line Testing
1. Plug in a regular analog phone
2. Go off-hook (pick up handset)
3. Should hear dial tone
4. Can dial using DTMF tones or rotary dial
5. Incoming calls should ring properly

### State Testing
1. `V=1` to enable verbose logging
2. Perform various operations:
   - Go off-hook → see state changes
   - Dial a number → see BT commands
   - Receive call → see ring signals
   - Hang up → see state transitions

## 8. Debugging

### Enable Verbose Output
```bash
# Via serial console:
V=1                  # Enable debug logging

# In code (edit main/main.c):
esp_log_level_set("*", ESP_LOG_DEBUG);  // Add this in app_main()
```

### Monitor Specific Signals
```bash
# Use oscilloscope on:
GPIO25 (DAC)   - Should see AC waveform for tones
GPIO32 (FR)    - Should see pulses during ringing
GPIO34 (SHK)   - Should go HIGH when phone off-hook
GPIO17 (BT TX) - UART data to BM64
```

### Serial Command Testing
Send these test commands:
```
H     # Help (should display menu)
R     # Reset BM64 (should see UART traffic)
P=14  # Send raw packet (0x14 = Event ACK)
```

## 9. Customization Examples

### Change BT Reconnection Timeout
Edit `components/bt_module/bt_module.c`:
```c
#define BT_RECONNECT_MSEC 30000  // Changed from 60000 (every 30s instead of 60s)
```

### Adjust Ring Frequency
Edit `components/pots_module/pots_module.c`:
```c
#define POTS_RING_FREQ_HZ 20     // Changed from 25 (slower ring pulse)
```

### Change Evaluation Rate
Edit respective module headers:
```c
#define BT_EVAL_MSEC 25          // More frequent BT updates
#define POTS_EVAL_MSEC 5         // More frequent POTS updates (beware: CPU load)
```

## 10. Next Steps

### Performance Tuning
- Monitor CPU usage in `idf.py monitor`
- Adjust task priorities in main.c if needed
- Consider reducing sample rate if audio not critical

### Reliability
- Add watchdog timer
- Implement error recovery
- Test with real BM64/AG1171 modules

### Production Enhancements
- Implement proper Goertzel algorithm for DTMF
- Add I2S with external DAC for better audio
- Implement electrical isolation for POTS line
- Add configuration web interface

## 11. Getting Help

### Check These First
- [README.md](README.md) - Overview and architecture
- [PIN_CONFIG.md](PIN_CONFIG.md) - Pin assignments
- [TROUBLESHOOTING.md](TROUBLESHOOTING.md) - Common issues

### Debug Commands
```bash
# View detailed help
idf.py build --help
idf.py -p /dev/ttyUSB0 monitor --help

# Check system info
esptool.py chip_id              # Verify ESP32 connection
esptool.py read_mac             # Read MAC address
esptool.py flash_id             # Check flash chip
```

### Serial Output Capture
```bash
# Save monitor output to file
idf.py -p /dev/ttyUSB0 monitor | tee output.log

# Filter specific messages
idf.py -p /dev/ttyUSB0 monitor | grep "BT_MODULE"
```

## 12. Important Warnings

⚠️ **Electrical Safety**
- POTS phone lines can carry lethal voltage when ringing (90V AC)
- Use proper isolation and protection circuits
- Never touch bare phone line connections during operation

⚠️ **Audio Coupling**
- Check audio levels before connecting real phone
- Start with low amplitudes to avoid oscillation
- Use proper isolation capacitors

⚠️ **Power Supply**
- Separate regulated supplies recommended
- Add filtering capacitors (100µF) on power rails
- Ensure stable 3.3V for all modules

---

**Happy building!** 🚀

For issues or questions, check the documentation or submit an issue on GitHub.
