# Architecture Overview - Blue POT ESP32

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                          ESP32-WROOM                            │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │                      FreeRTOS Kernel                       │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │  │
│  │  │ POTS Task    │  │ BT Task      │  │ CMD Task     │    │  │
│  │  │ Priority +2  │  │ Priority +1  │  │ Default      │    │  │
│  │  │ Every 10ms   │  │ Every 20ms   │  │ Every 10ms   │    │  │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘    │  │
│  │         │                 │                 │             │  │
│  │  ┌──────▼──────────────────▼──────────────────▼────────┐  │  │
│  │  │              Core Application Logic                 │  │  │
│  │  ├────────────────────────────────────────────────────┤  │  │
│  │  │ ┌──────────────┐   ┌──────────────┐  ┌──────────┐ │  │  │
│  │  │ │ POTS Module  │   │ BT Module    │  │ Command  │ │  │  │
│  │  │ ├──────────────┤   ├──────────────┤  │Processor │ │  │  │
│  │  │ │ • Hook SW    │   │ • HCI Packet │  ├──────────┤ │  │  │
│  │  │ │ • Ring Gen   │   │ • State Mach │  │ • Serial │ │  │  │
│  │  │ │ • Tone Gen   │   │ • Dialing    │  │ • Config │ │  │  │
│  │  │ │ • DTMF Det   │   │ • Call Mgmt  │  │ • NVS    │ │  │  │
│  │  │ └──────────────┘   └──────────────┘  └──────────┘ │  │  │
│  │  └────────────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                   │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐       │
│  │   UART2      │    │  GPIO        │    │  ADC/DAC     │       │
│  │  115200bps   │    │  (32 pins)   │    │  (12-bit)    │       │
│  └──────┬───────┘    └──────┬───────┘    └──────┬───────┘       │
└─────────┼──────────────────┼──────────────────┼────────────────┘
          │                  │                  │
          ▼                  ▼                  ▼
     ┌─────────┐      ┌──────────┐      ┌──────────┐
     │  BM64   │      │ AG1171   │      │  Audio   │
     │Bluetooth│      │  SLIC    │      │Coupling  │
     │ Module  │      │Interface │      │Circuits  │
     └─────────┘      └──────────┘      └──────────┘
          │                  │
          ▼                  ▼
   ┌─────────────────────────────┐
   │   Bluetooth Phone          │
   │   (Connected via BM64)      │
   └─────────────────────────────┘
          │
          │          ┌────────────────┐
          └─────────►│  Analog Phone  │
                     │  (POTS Line)   │
                     └────────────────┘
```

## Component Interaction Flow

### Incoming Call Scenario
```
1. BM64 receives call → sends Call_Status packet
2. BT Module processes: bt_call_state = CALL_INCOMING
3. BT Module calls: pots_set_ring(true)
4. POTS Module starts ring pulse generator (GPIO32/33)
5. Phone rings (user hears AC line ringing)
6. User goes off-hook (GPIO34 becomes HIGH)
7. POTS Module detects hook change, stops ringing
8. BT Module detects state change via pots_hook_change()
9. BT Module sends accept_call() to BM64
10. Audio path activated (BM64 AOHPR ↔ AG1171 VIN/VOUT)
11. BM64 sends: bt_call_state = CALL_ACTIVE
12. Call connected, audio flows both directions
```

### Outgoing Call Scenario
```
1. User goes off-hook (GPIO34 → HIGH)
2. POTS Module detects and sets state = OFF_HOOK
3. POTS Module generates dial tone (GPIO25 DAC output)
4. BT Module detects pots_hook_change() 
5. BT Module sets: bt_state = BT_DIALING
6. User dials digits (DTMF or rotary)
7. POTS Module detects each digit via pots_digit_dialed()
8. BT Module collects 10 digits
9. BT Module sends dial_number() command to BM64
10. BM64 initiates outgoing call (bt_call_state = CALL_OUTGOING)
11. Phone line goes quiet (no tone), user hears call progress
12. Call connects (bt_call_state = CALL_ACTIVE)
13. Audio flows, conversation proceeds
```

## State Machine Interaction

### Bluetooth State Machine (20ms evaluation)
```
DISCONNECTED
  │ (bt_in_service from BM64)
  ▼
CONNECTED_IDLE ◄─────────┐
  │ (off-hook)           │
  ▼                      │ (call ends)
DIALING                  │
  │ (10 digits or 0)     │
  ▼                      │
CALL_INITIATED           │
  ├─→ CALL_OUTGOING ─────┘
  │
  └─→ CALL_ACTIVE ◄─────┐
      │                 │ (phone goes off-hook)
      └─ CALL_RECEIVED ─┘
              │
         (on-hook triggers state change)
```

### POTS State Machine (10ms evaluation)
```
ON_HOOK
  ├─→ OFF_HOOK (on hook change, off-hook detected)
  │     ├─ Starts tone generation
  │     ├─ Waits for hook change OR
  │     └─ Waits for ringing to start
  │
  └─→ RINGING (external ring command)
      └─ Returns to ON_HOOK when:
         • User picks up (goes off-hook), OR
         • Ringing stops (call not answered)
```

## Task Scheduling

### Priority Levels (Lower number = Lower priority)
```
Default (0)     ← Command Processor Task
      ↓         (Serial I/O, NVS access)
      
Priority +1     ← Bluetooth Task (20ms interval)
      ↓         (UART RX from BM64, state machine)
      
Priority +2     ← POTS Task (10ms interval)
                (GPIO sampling, tone generation, ADC)
```

### Timing

| Module | Interval | Rate | Critical Operations |
|--------|----------|------|---------------------|
| POTS | 10ms | 100Hz | Hook detection, tone generation, ring pulses |
| BT | 20ms | 50Hz | BM64 communication, state transitions |
| CMD | 10ms | 100Hz | Serial parsing (can be slower) |

## Data Flow

### POTS → BT Communication
```c
pots_hook_change() / pots_digit_dialed()
    ↓
POTS Module provides state info
    ↓
BT Module queries during eval:
  - if (pots_hook_change(&off_hook)) { ... }
  - if (pots_digit_dialed(&digit)) { ... }
```

### BT → POTS Communication
```c
pots_set_in_service(bool)
pots_set_ring(bool)
pots_set_in_call(bool)
    ↓
POTS Module adjusts:
  - Tone generation (dial, busy, off-hook)
  - Ring pulse generation
  - Hook detection sensitivity
```

### Serial → NVS → Application
```c
Serial Command: D=2
    ↓
CMD Processor calls: bt_set_pairing_number(2)
    ↓
Also stores in NVS: nvs_set_i32()
    ↓
Persists across power cycles
    ↓
On reboot, read NVS and pass to bt_module_init()
```

## Signal Integrity

### Audio Signal Paths

#### DAC Output (Tone Generation)
```
DAC2 (0-3.3V, ~0.5V AC amplitude)
  → 1µF AC coupling capacitor
  → AG1171 VIN (expects 0.5V-1V amplitude)
  → SLIC amplification
  → Phone line (up to 20V amplitude on line)
```

#### ADC Input (DTMF Detection)
```
AG1171 VOUT (audio output from SLIC)
  → 1µF AC coupling capacitor
  → Bias circuit (sets DC offset to ~1.65V)
  → GPIO35 ADC (0-3.3V input range)
  → Software processing (Goertzel or threshold)
```

## Performance Characteristics

### CPU Usage (Estimated)
- POTS Task: 5-10% (GPIO polling, basic tone generation)
- BT Task: 2-5% (Serial I/O, packet parsing)
- CMD Task: <1% (Serial input, slow NVS access)
- **Total: ~10-15% average**

### Memory Usage (Estimated)
- Stack per task: 2KB (current allocation)
- Bluetooth buffers: ~1KB
- POTS buffers: ~500B
- **Total heap: ~10-20KB**

### UART Throughput
- BM64 UART: 115200 bps (theoretical: 14.4 KB/s)
- Packet size: 5-30 bytes
- Rate: 20-50 packets/second depending on state

## Synchronization

### No Mutexes/Semaphores Used
- POTS and BT tasks access different GPIO/hardware
- Shared data structures accessed by one task at a time
- Command processor only writes to NVS (safe with FreeRTOS)
- **Rationale**: Low contention, simple state variables

### Future Improvements for Production
- Add mutex protection for bt_state/bt_in_service
- Use event groups for inter-task synchronization
- Consider queue for UART RX data if higher throughput needed

---

See the individual component documentation for more details:
- [bt_module.h](components/bt_module/include/bt_module.h)
- [pots_module.h](components/pots_module/include/pots_module.h)
- [cmd_processor.h](main/include/cmd_processor.h)
