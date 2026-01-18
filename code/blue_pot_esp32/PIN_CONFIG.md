# Pin Configuration Guide

## ESP32-WROOM Default GPIO Assignments

### BM64 Bluetooth Module Connection

| ESP32 GPIO | Pin Name | Direction | BM64 Pin | Function |
|-----------|----------|-----------|----------|----------|
| GPIO16 | D16 | ← | TX | UART Receive from BM64 |
| GPIO17 | D17 | → | RX | UART Transmit to BM64 |
| GPIO18 | D18 | → | RSTN | Reset (Active Low) |
| GPIO19 | D19 | ↔ | EAN | EAN Configuration |
| GPIO21 | D21 | ↔ | P2_0 | P2_0 Configuration |
| GPIO22 | D22 | → | MFB | Multi-Function Button |

### AG1171 SLIC Telephone Interface

| ESP32 GPIO | Pin Name | Direction | AG1171 Pin | Function |
|-----------|----------|-----------|-----------|----------|
| GPIO32 | D32 | → | FR | Tip/Ring Forward/Reverse |
| GPIO33 | D33 | → | RM | Ring Mode Control |
| GPIO34 | D34 | ← | SHK | Switch Hook Detection (Input) |
| GPIO25 | D25 | → | VIN | Tone Output (DAC2) |
| GPIO35 | D35 | ← | VOUT | Phone Line Input (ADC1_CH7) |

### Indicator and Misc

| ESP32 GPIO | Function | Notes |
|-----------|----------|-------|
| GPIO25 | Off-Hook LED or DAC2 | Can't use both - use GPIO for LED if using external DAC |
| GPIO1 | UART0 TX (Console) | USB Serial via UART0 |
| GPIO3 | UART0 RX (Console) | USB Serial via UART0 |

## BM64 Mode Configuration

The BM64 mode is determined by pin states during reset:

| EAN | P2_0 | MFB | Mode | Purpose |
|-----|------|-----|------|---------|
| L   | L    | L   | 1    | Flash IBDK (Firmware Update) |
| H   | H    | -   | 2    | ROM App (Not normally used) |
| H   | L    | -   | 3    | ROM IBDK (Not normally used) |
| L   | H    | -   | 0    | Flash App (Normal Operation) |

**Current configuration**: Mode 0 (Normal Flash App operation)

## Audio Signal Paths

### Tone Output (ESP32 → Phone Line)
```
GPIO25 (DAC2)
  ↓ (0-3.3V waveform, ~200µA max)
  1µF coupling capacitor
  ↓ (AC signal ~0-3.3V centered at ~1.65V bias)
AG1171 VIN
  ↓ (amplified to line level)
Phone Line (Tip/Ring)
```

### Phone Input (Phone Line → ESP32)
```
AG1171 VOUT
  ↓ (audio output from SLIC)
1µF coupling capacitor + 0.6V bias circuit
  ↓ (AC signal biased for ADC input range)
GPIO35 (ADC1_CH7)
  ↓ (ADC samples at 8kHz)
Software (DTMF detection)
```

### Bluetooth Audio Path
```
Phone Microphone → AG1171 VOUT → 0.1µF cap + voltage divider → BM64 MIC_P1
BM64 AOHPR → 1µF cap → AG1171 VIN
```

## Electrical Considerations

### Power Supply
- Separate regulated 3.3V supplies recommended:
  - ESP32: 500mA capable
  - BM64: 400mA peak
  - AG1171: 200mA typical

### Protection
- Add 100nF bypass capacitors near VCC pins
- Consider TVS diodes on external signal inputs
- Add series resistors (1kΩ) on GPIO outputs to external modules

### Grounding
- Star ground topology preferred
- All signal grounds connected at single point
- Separate analog ground for AG1171 if possible
- No ground loops

## Pin Customization

To change pin assignments:

1. Edit [main/include/blue_pot.h](main/include/blue_pot.h):
```c
#define PIN_BT_RX    16  // Change these values
#define PIN_BT_TX    17
// ... etc
```

2. Update GPIO usage in:
   - `components/bt_module/bt_module.c` (_bt_init_pins)
   - `components/pots_module/pots_module.c` (_pots_init_hardware)

3. Rebuild project:
```bash
idf.py build
```

## Development Board Pinouts

### Common ESP32 Development Boards

#### ESP32 DevKit-C
```
       USB
       |||
EN  |         | GND
IO36|  ESP32  | IO23
IO39|         | IO22
IO34| WROVER  | IO1 (TX)
IO35|         | IO3 (RX)
IO32| with    | IO21
IO33| PSRAM   | GND
IO25|         | IO19
IO26|         | IO18
IO27|         | IO5
IO14|         | IO17
IO12|         | IO16
GND |         | IO4
IO13|         | IO2
SD2 |         | IO15
SD3 |         | IO8
CMD |         | IO7
CLK |         | IO6
SD0 |         | GND
SD1 |         | IO9
GND |         | IO10
        
```

## Testing Connection

To verify GPIO connections work before running full firmware:

```python
# test_pins.py - Simple GPIO test
import time
from machine import Pin

# Test output pins
pins_out = [(18, 'RST'), (19, 'EAN'), (21, 'P2_0'), (22, 'MFB'), (32, 'FR'), (33, 'RM')]
pins_in = [(34, 'SHK'), (35, 'VOUT')]

for pin, name in pins_out:
    p = Pin(pin, Pin.OUT)
    p.on()
    print(f"{name} (GPIO{pin}): ON")
    time.sleep(0.1)
    p.off()
    print(f"{name} (GPIO{pin}): OFF")
    time.sleep(0.1)

for pin, name in pins_in:
    p = Pin(pin, Pin.IN)
    print(f"{name} (GPIO{pin}): {p.value()}")
```

## Recommended Connector Pinouts

### For BM64 Module (example with standard DIP header)
```
Pin 1: VCC    (3.3V)
Pin 2: GND
Pin 3: TX     → ESP32 GPIO16
Pin 4: RX     ← ESP32 GPIO17
Pin 5: RSTN   ← ESP32 GPIO18
Pin 6: EAN    ↔ ESP32 GPIO19
Pin 7: P2_0   ↔ ESP32 GPIO21
Pin 8: MFB    ← ESP32 GPIO22
```

### For AG1171 Module (example connection)
```
Pin 1: VCC    (3.3V)
Pin 2: GND
Pin 3: FR     ← ESP32 GPIO32
Pin 4: RM     ← ESP32 GPIO33
Pin 5: SHK    → ESP32 GPIO34
Pin 6: VIN    ← ESP32 GPIO25 (via 1µF cap)
Pin 7: VOUT   → ESP32 GPIO35 (via 1µF cap + bias)
Pin 8: TIP    (to phone line)
Pin 9: RING   (to phone line)
```
