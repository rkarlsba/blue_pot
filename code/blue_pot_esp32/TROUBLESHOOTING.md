# Troubleshooting Guide for Blue POT ESP32

## Common Issues

### 1. BM64 Module Not Responding

**Symptoms**: "Connection attempt" messages but BM64 never connects

**Solutions**:
- Verify UART pins (GPIO16/17) are correctly connected
- Check baud rate (should be 115200)
- Ensure BM64 has stable 3.3V power supply
- Verify reset pin (GPIO18) toggling with `bt_do_reset()`
- Check if BM64 is in correct mode (Flash App = mode 0)

### 2. Phone Hook Detection Not Working

**Symptoms**: Off-hook LED doesn't light, no tone generation

**Solutions**:
- Verify GPIO34 (SHK) connection to AG1171
- GPIO34 is ADC-safe, but check for electrical noise
- Test with multimeter: should go HIGH when phone off-hook
- Check debounce timing in `POTS_ON_HOOK_DETECT_MSEC`

### 3. DTMF Tones Not Detected

**Symptoms**: DTMF digits not recognized, rotary dial works

**Solutions**:
- Current implementation is simplified - needs improvement
- Implement Goertzel algorithm for better detection
- Check ADC sampling (GPIO35 input from AG1171)
- Verify audio coupling capacitor (1µF) between VOUT and ADC

### 4. Tone Output Weak or Silent

**Symptoms**: Dial tone not heard on phone line

**Solutions**:
- Verify DAC2 (GPIO25) connection
- Check coupling capacitor (1µF) to AG1171 VIN
- Confirm AG1171 is in correct mode (FR=HIGH for normal operation)
- Measure voltage on AG1171 VIN with oscilloscope
- Check sample rate and amplitude settings

### 5. Bluetooth Pairing Issues

**Symptoms**: Pairing fails, BM64 doesn't enter pairing mode

**Solutions**:
- Send `L` command to enable pairing mode
- BM64 will timeout after ~2 minutes
- Verify BM64 is not already paired to a device
- Try factory reset of BM64 if needed

### 6. Serial Commands Not Responding

**Symptoms**: No response to `H`, `D`, etc.

**Solutions**:
- Check USB serial connection (baud = 115200)
- Verify USB-to-Serial adapter driver installed
- Try `idf.py monitor` instead of external terminal
- Confirm command format (uppercase, terminator = CR or LF)

## Hardware Debugging

### Checking Connections

```
# Verify UART2 to BM64
GPIO16 (RX) <- measure TX signal from BM64
GPIO17 (TX) -> measure with oscilloscope (should see packets)

# Verify POTS signals
GPIO32 (FR) -> measure with multimeter (should toggle 0-3.3V for ringing)
GPIO33 (RM) -> measure (HIGH during ring mode)
GPIO34 (SHK) <- measure input (HIGH when off-hook)
GPIO25 (DAC) -> measure with oscilloscope (should see AC waveform for tones)
GPIO35 (ADC) <- verify connection to AG1171 VOUT
```

### Oscilloscope Capture Tips

- Dial tone: 350Hz + 440Hz combined waveform
- No service tone: 480Hz + 620Hz, 300ms on, 200ms off
- Receiver off-hook: 4-frequency burst (1400, 2060, 2450, 2600 Hz)

## Firmware Recovery

If the ESP32 is bricked:

```bash
# Erase all flash
esptool.py -p /dev/ttyUSB0 erase_flash

# Build and flash fresh
cd blue_pot_esp32
idf.py clean
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Performance Tuning

### Task Priorities
Current settings (can be adjusted in main.c):
- POTS task: +2 (most critical, every 10ms)
- BT task: +1 (medium, every 20ms)
- CMD task: 0 (lowest, every 10ms)

### Increasing DTMF Accuracy
Edit `pots_module.c`:
```c
#define POTS_DTMF_DRIVEN_MSEC 50  // Increase from 30 for better accuracy
#define POTS_DTMF_SILENT_MSEC 50   // Increase from 30
```

### Reducing Power Consumption
- Disable verbose logging (`V=0`)
- Reduce sample rate if audio quality permits
- Enable power management in sdkconfig.defaults

## Testing Checklist

- [ ] BM64 UART communication verified
- [ ] Bluetooth pairing successful
- [ ] Phone hook switch detection working
- [ ] Dial tone generated on off-hook
- [ ] Ring pulse output correct frequency/timing
- [ ] DTMF detection working (rotary OK too)
- [ ] Serial commands responsive
- [ ] Pairing ID persists after reboot
- [ ] All state transitions smooth

## Getting Help

1. Check `idf.py monitor` output for error messages
2. Enable verbose logging (`V=1`)
3. Monitor GPIO pin states with oscilloscope
4. Check power supply stability
5. Verify component datasheets match pin assignments

## Useful Commands

```bash
# View real-time logs with timestamps
idf.py -p /dev/ttyUSB0 monitor -t timestamp

# Full reset and rebuild
idf.py fullclean && idf.py build

# Check NVS partition data
esptool.py -p /dev/ttyUSB0 read_flash 0x9000 0x6000 nvs_data.bin

# Capture serial output to file
idf.py -p /dev/ttyUSB0 monitor | tee output.log
```
