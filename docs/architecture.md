# Crunch Koskalak — Architecture Reference

## MQTT Topic Structure

```
crunch/{orgId}/{locationId}/{deviceId}/telemetry   — Device → Server (heartbeat, battery, signal)
crunch/{orgId}/{locationId}/{deviceId}/events       — Server → Device (order status, inventory alerts)
crunch/{orgId}/{locationId}/{deviceId}/commands      — Server → Device (config push, OTA trigger)
crunch/{orgId}/{locationId}/{deviceId}/config        — Server → Device (personality, volume, wake word)
```

## Device State Machine

```
                    ┌──────────────┐
                    │   BOOTING    │
                    └──────┬───────┘
                           │
                    ┌──────┴───────┐
              ┌─────│ PROVISIONING │ (BLE/SmartConfig)
              │     └──────┬───────┘
              │            │ (credentials received)
              │     ┌──────┴───────┐
              │     │  CONNECTING  │ (WiFi + MQTT)
              │     └──────┬───────┘
              │            │ (connected)
              │     ┌──────┴───────┐
              ├────►│    IDLE      │◄───────────────┐
              │     └──┬───────┬───┘                │
              │        │       │                    │
              │  (wake word) (timeout)              │
              │        │       │                    │
              │  ┌─────┴──┐  ┌─┴──────────┐        │
              │  │LISTENING│  │  SLEEPING  │        │
              │  └─────┬──┘  └────────────┘        │
              │        │ (end of speech)            │
              │  ┌─────┴──┐                         │
              │  │THINKING │ (waiting for Sage)      │
              │  └─────┬──┘                         │
              │        │ (response received)        │
              │  ┌─────┴──┐                         │
              │  │SPEAKING │ (TTS + animation)       │
              │  └─────┬──┘                         │
              │        │ (finished)                 │
              │        └────────────────────────────┘
              │
              │  ┌──────────┐
              └─►│ UPDATING │ (OTA in progress)
                 └──────────┘
```

## Voice Session Sequence

```
Guest                   Gadget (ESP32)           server-gadget          OpenAI         server-chatkit
  │                         │                         │                    │                 │
  │──speaks──────────────►  │                         │                    │                 │
  │                         │ (wake word detected)    │                    │                 │
  │                         │──WS connect────────────►│                    │                 │
  │                         │                         │                    │                 │
  │                         │──PCM16 audio frames───► │                    │                 │
  │                         │──PCM16 audio frames───► │                    │                 │
  │                         │──{end_of_speech}──────► │                    │                 │
  │                         │                         │──Whisper API──────►│                 │
  │                         │                         │◄──transcript───────│                 │
  │                         │◄─{transcript: "..."}────│                    │                 │
  │                         │                         │──POST /api/chat───────────────────► │
  │                         │                         │                    │  (Sage + MCP)   │
  │                         │                         │◄──response text────────────────────  │
  │                         │◄─{response: "..."}──────│                    │                 │
  │                         │                         │──TTS API──────────►│                 │
  │                         │                         │◄──PCM16 audio──────│                 │
  │                         │◄──PCM16 audio───────────│                    │                 │
  │◄──speaks (TTS)──────────│                         │                    │                 │
  │◄──animation─────────────│                         │                    │                 │
  │                         │──WS close──────────────►│                    │                 │
```

## ESP32-S3 Memory Budget

| Allocation | SRAM (512KB) | PSRAM (8MB) |
|------------|-------------|-------------|
| FreeRTOS kernel + tasks | ~65KB | — |
| WiFi + TCP/IP stack | ~80KB | — |
| MQTT client (TLS) | ~20KB | — |
| WebSocket client (TLS, on-demand) | ~50KB | — |
| ESP-SR wake word model | ~30KB | ~200KB |
| Audio DMA buffers (I2S) | ~16KB | — |
| IMU driver (100Hz task) | ~5KB | — |
| Balance controller (50Hz task) | ~3KB | — |
| Touch driver (10Hz task) | ~2KB | — |
| LED driver (30Hz task) | ~2KB | — |
| Battery monitor (0.1Hz task) | ~2KB | — |
| LVGL display buffer (240x320x2) | — | ~150KB |
| LVGL widget/style heap | — | ~100KB |
| Application heap | ~40KB | ~500KB |
| **Free** | **~197KB** | **~7MB** |

PSRAM is used for large allocations (display buffers, audio processing buffers).
SRAM is reserved for stack, DMA, and interrupt-critical data.

## FreeRTOS Task Map

| Task | Core | Priority | Rate | Stack | Purpose |
|------|------|----------|------|-------|---------|
| `state_machine` | 0 | 5 | Event-driven | 4096 | Device state FSM |
| `imu` | 0 | 6 | 100Hz | 4096 | MPU-6050 sampling + orientation + gesture detection |
| `balance` | 0 | 5 | 50Hz | 3072 | PD balance controller → shared offset buffer |
| `motion` | 0 | 4 | 50Hz | 4096 | Gait oscillator + balance offsets → PCA9685 servos |
| `touch` | 0 | 2 | 10Hz | 2048 | Capacitive touch zone event detection |
| `led` | 0 | 1 | 30Hz | 2048 | WS2812 pattern animation |
| `battery` | 0 | 1 | 0.1Hz | 2048 | ADC voltage monitoring |
| `audio_capture` | 1 | 7 | Continuous | 4096 | I2S mic DMA + wake word + streaming |
| `audio_playback` | 1 | 7 | Continuous | 4096 | I2S speaker DMA + TTS playback |
| `lvgl_tick` | 0 | 3 | 30Hz | 4096 | LVGL display refresh |

Core 0: state machine, sensors, motion, UI cosmetics
Core 1: audio (time-critical DMA — isolated from servo/sensor jitter)

## IMU + Balance Architecture

Inspired by KT2's TurtleOS: balance runs as a standalone OS-level service, invisible to gait/animation code. Three independent FreeRTOS tasks collaborate via shared buffers.

```
  ┌──────────────┐
  │   MPU-6050   │ (I2C, shared bus with PCA9685)
  │   6-axis IMU │
  └──────┬───────┘
         │
         │ I2C reads at 100Hz
         │
  ┌──────┴───────────┐   IMU TASK (Core 0, Pri 6, 100Hz)
  │                   │
  │  Complementary    │
  │  filter           │──► pitch, roll (shared data)
  │  (0.98 gyro +     │
  │   0.02 accel)     │──► Gesture events → FSM callbacks
  │                   │      (pickup, shake, tap, tip-over)
  └───────────────────┘
         │
         │ reads pitch/roll
         │
  ┌──────┴───────────┐   BALANCE TASK (Core 0, Pri 5, 50Hz)
  │                   │
  │  PD Controller    │
  │  P=1.5, D=0.05   │──► offset buffer [N servos] (shared)
  │  deadband=1°      │
  │  max=±15°         │   Supports 4-DOF and 8-DOF via KOSKALAK_DOF
  └───────────────────┘
         │
         │ reads offset buffer
         │
  ┌──────┴───────────┐   MOTION TASK (Core 0, Pri 4, 50Hz)
  │                   │
  │  Gait oscillator  │
  │  + balance offsets │──► PCA9685 I2C writes → servos
  │                   │
  │  final_pos =      │   Suppressed when imu_is_held() == true
  │    gait_offset    │
  │    + gait_amp     │
  │      * sin(...)   │
  │    + balance_off  │
  └───────────────────┘
```

### Gesture Events (IMU)

| Event | Trigger | Action |
|-------|---------|--------|
| PICKUP | Sustained <0.3g for >100ms | Suppress all gait patterns, solid cyan LEDs |
| PUT_DOWN | Impact after held state | Resume idle gait, breathing white LEDs |
| TIP_OVER | Pitch or roll >45 degrees | Emergency crouch, flash amber LEDs |
| SHAKE | 3+ high-gyro spikes in 1s | Emit "call server" event via MQTT, pulse green |
| TAP | Accel spike >2g, <50ms | Wake from sleep |
| SURFACE_TILT | Static tilt >5 degrees | Continuous balance correction (no event) |

### Touch Events

| Event | Zone | Action |
|-------|------|--------|
| TAP | HEAD | Wake from sleep, greeting hop |
| DOUBLE_TAP | HEAD | Celebrate animation + rainbow LEDs |
| LONG_PRESS | BACK | Enter sleep mode |
| HOLD_START | HEAD | Push-to-talk: start voice capture (noisy environment fallback) |
| HOLD_END | HEAD | Push-to-talk: end voice capture, send to pipeline |

### 4-DOF vs 8-DOF Trade-Off

| Factor | 8-DOF (default) | 4-DOF (KT2-style) |
|--------|------------------|--------------------|
| Servos | 8x MG90S ($32) | 4x MG90S ($16) |
| Expressiveness | Rich static poses, precise leg placement | Dynamic momentum gaits, less pose variety |
| Balance | Compensate via hip offsets (knees unaffected) | All correction through single joint per leg |
| Power draw | ~2A peak (all servos moving) | ~1A peak |
| Chassis | miniKame parallelogram linkage (proven) | Requires new chassis design |
| Firmware | Direct port from miniKame gait library | New gait library needed (momentum-based) |
| Risk | Low (proven platform) | Medium (less tested, needs IMU feedback to work well) |

**Recommendation:** Prototype both. Start with 8-DOF (proven). Build a 4-DOF variant in Month 2 using the same firmware (just change KOSKALAK_DOF=4). Let pilot testing determine which form factor restaurants prefer.

## Compliance Reference

### FCC (United States)
- Use pre-certified ESP32-S3-WROOM-1 module (FCC ID already granted for the module)
- Host product only needs **Subpart B** unintentional radiator testing (~$3,000-5,000)
- If using bare ESP32-S3 chip instead: full intentional radiator testing (~$15,000-25,000)
- **Decision: WROOM module is mandatory for production**

### CE (EU/EEA)
- Radio Equipment Directive (RED) 2014/53/EU
- Pre-certified module simplifies RF compliance (same as FCC)
- Host product needs EMC + safety testing

### Battery Safety
- **UN 38.3:** Mandatory for air/sea shipping of lithium cells. Source cells with existing UN 38.3 certification to avoid $2,000-5,000 per-cell-type testing + 4-8 week lead time.
- **IEC 62133-2:** Operational safety standard for rechargeable lithium cells. Required in most markets.
- **Procurement rule:** Only source battery cells from suppliers who can provide UN 38.3 test reports.

### Toy Safety (if applicable)
- If marketed to children: ASTM F963 (US) / EN 71 (EU) apply
- Current positioning as B2B restaurant hardware avoids toy classification
- Keep marketing materials business-focused to maintain this classification
