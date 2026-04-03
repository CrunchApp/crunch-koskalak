# Crunch Koskalak - CLAUDE.md

## Project Overview

Crunch Koskalak is the **software stack for Crunch Gadget** — an ESP32-S3-based tabletop robot for restaurant front-of-house operations. This repo contains three software components:

1. **firmware/** — ESP-IDF C/C++ firmware for the ESP32-S3 device
2. **server-gadget/** — Node.js/TypeScript gateway service bridging device ↔ Crunch platform
3. **agent/** — Sage agent definition (Python) for integration into `server-chatkit`

Hardware design (3D models, PCB schematics) lives in a separate repo. This project is software-only.

## Architecture

```
Guest speaks → ESP32-S3 (wake word detection on-device)
    → Audio streamed via WebSocket to server-gadget
    → server-gadget proxies to OpenAI Whisper (STT)
    → Transcript sent to server-chatkit (Sage agent)
    → Sage uses MCP tools (server-ops-mcp, server-square-mcp)
    → Response text → OpenAI TTS → audio streamed back to device
    → Device plays audio + expressive animation

Persistent connection: MQTT (telemetry, OTA triggers, events)
On-demand connection: WebSocket (voice sessions only)
```

### Integration Points with Crunch Ecosystem

| Crunch Service | How Koskalak Uses It | Protocol |
|----------------|---------------------|----------|
| server-ops (port 3000) | Menu, orders, inventory, locations via REST API | HTTPS |
| server-ops (webhooks) | Receives order.created, inventory.updated events | HTTPS → MQTT fan-out |
| server-chatkit (port 8001) | Sage agent for voice conversations | HTTP |
| server-ops-mcp (port 8787) | AI tool access to all Crunch data | Streamable HTTP |
| server-square-mcp (port 8788) | Square POS data (payments, customers) | Streamable HTTP |
| web-platform (port 3001) | Devices dashboard section for fleet management | N/A (UI changes in web-platform repo) |

### Device Authentication

Gadgets register as Crunch App Store apps. Each device gets a JWT scoped to `organizationId + locationId` via QR-code provisioning flow. The gateway (server-gadget) issues and validates these tokens.

## Repo Structure

```
crunch-koskalak/
├── firmware/              — ESP-IDF project (C/C++)
│   ├── CMakeLists.txt     — Top-level ESP-IDF project file
│   ├── sdkconfig.defaults — Default Kconfig values
│   ├── partitions.csv     — Dual OTA partition table
│   ├── main/              — Main application code
│   │   ├── main.c         — Entry point (app_main)
│   │   ├── audio/         — I2S mic/speaker, ESP-ADF pipeline, wake word
│   │   ├── display/       — LVGL driver for ST7789 2.4" LCD
│   │   ├── imu/           — MPU-6050 driver, orientation + gesture detection
│   │   ├── led/           — WS2812 NeoPixel driver via RMT peripheral
│   │   ├── motion/        — Servo control, gait engine, standalone balance controller
│   │   ├── network/       — WiFi manager, MQTT client, WebSocket client
│   │   ├── power/         — Battery ADC monitor (1S/2S configurable)
│   │   ├── touch/         — ESP32-S3 native capacitive touch (push-to-talk fallback)
│   │   └── device/        — State machine, OTA updater, config store
│   └── components/        — Custom ESP-IDF components
├── server-gadget/         — Gateway service (Node.js/TypeScript)
│   ├── package.json
│   ├── tsconfig.json
│   ├── src/
│   │   ├── index.ts       — Express server entry point
│   │   ├── config.ts      — Environment configuration
│   │   ├── routes/        — HTTP + WebSocket route handlers
│   │   ├── services/      — Business logic (voice proxy, device registry, OTA, events)
│   │   ├── middleware/     — Device JWT auth, rate limiting
│   │   └── types/         — TypeScript type definitions
│   └── tests/             — Vitest test files
└── agent/                 — Sage agent for server-chatkit
    ├── sage_instructions.py   — System prompt (voice-optimised, FOH-scoped)
    ├── sage_capabilities.py   — Capability registry additions
    └── README.md              — Integration guide for server-chatkit
```

## Dev Commands

### Firmware (ESP-IDF)

Requires ESP-IDF v5.x toolchain installed. See https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/

```bash
cd firmware

# Configure (first time or after sdkconfig changes)
idf.py set-target esp32s3
idf.py menuconfig

# Build
idf.py build

# Flash to device (adjust port as needed)
idf.py -p /dev/tty.usbserial-* flash

# Monitor serial output
idf.py -p /dev/tty.usbserial-* monitor

# Build + flash + monitor in one
idf.py -p /dev/tty.usbserial-* flash monitor
```

### Server-Gadget (Node.js)

```bash
cd server-gadget

# Install dependencies
npm install

# Development (hot reload)
npm run dev

# Build
npm run build

# Production
npm start

# Test
npm test

# Lint
npm run lint

# Type check
npx tsc --noEmit
```

### Agent (Python — integration into server-chatkit)

The agent/ directory contains files meant to be copied/integrated into `../server-chatkit/app/agents/`. See `agent/README.md` for the integration steps.

### Running with Crunch Platform

From the Crunch root directory:
```bash
# Start all core Crunch services + gadget gateway
npm run dev           # Starts server-ops, web-platform, server-chatkit, MCPs
cd crunch-koskalak/server-gadget && npm run dev  # Start gadget gateway on port 3004
```

## Testing

```bash
# server-gadget (Vitest)
cd server-gadget && npm test

# Firmware — unit tests run on host (not device)
cd firmware && idf.py -T main build   # Build with test components
```

### Test Priorities
- Gateway service: unit tests for device auth, voice proxy routing, event fan-out logic
- Firmware: host-based tests for state machine logic, protocol parsers, config handling
- Integration: test gadget→gateway→server-ops flow with mock device

## Critical Rules

### Do
- Keep server-gadget as thin as possible — it proxies, it doesn't duplicate server-ops logic
- Validate `organizationId + locationId` on every device request (multi-tenant safety)
- Use MQTT for persistent device connections, WebSocket only for voice sessions
- Follow ESP-IDF coding conventions in firmware (FreeRTOS tasks, component pattern)
- Follow server-ops patterns in server-gadget (Express + TypeScript, same middleware style)
- Keep Sage agent responses under 30 words — it's voice-first, not chat-first
- Test servo control changes on physical hardware before committing — gait math is fragile

### Don't
- Don't put AI/agent logic in server-gadget — that belongs in server-chatkit (Sage agent)
- Don't connect ESP32 directly to OpenAI APIs — always proxy through server-gadget
- Don't store guest PII on the device — the ESP32 has no secure enclave for data-at-rest
- Don't add new endpoints to server-ops for gadget features — use server-gadget as the intermediary
- Don't use Arduino framework — this project uses ESP-IDF natively for RTOS control
- Don't run full firmware builds in sandbox/CI without ESP-IDF toolchain — use `npx tsc --noEmit` for gateway checks

## Environment Variables

### server-gadget/.env
```
PORT=3004
NODE_ENV=development
CRUNCH_API_URL=http://localhost:3000/api/v1
CRUNCH_CHATKIT_URL=http://localhost:8001
CRUNCH_MCP_URL=http://localhost:8787
OPENAI_API_KEY=sk-...
MQTT_BROKER_URL=mqtt://localhost:1883
DEVICE_JWT_SECRET=<random-secret-for-device-tokens>
OTA_FIRMWARE_PATH=./ota-binaries
```

### firmware sdkconfig.defaults
WiFi credentials and server URLs are provisioned at runtime via BLE/SmartConfig, not compiled in.

## Hardware Reference (For Context)

The device this software runs on:
- **MCU:** ESP32-S3-WROOM-1 (N16R8) — dual-core 240MHz, 512KB SRAM, 8MB PSRAM, 16MB flash. **Must use pre-certified module** for simplified FCC compliance (Subpart B only).
- **Display:** 2.4" IPS LCD (ST7789, 240x320, SPI)
- **Audio:** INMP441 I2S MEMS mic + MAX98357A I2S amp + 3W speaker
- **IMU:** MPU-6050 6-axis accelerometer/gyroscope (I2C, shared bus with PCA9685)
- **Touch:** ESP32-S3 native capacitive touch (4 zones, no external IC)
- **LEDs:** WS2812 NeoPixel ring (8 LEDs via RMT peripheral, single GPIO)
- **Servos:** 8x MG90S via PCA9685 16-ch PWM driver (I2C) — default config. 4-DOF config also supported (compile-time KOSKALAK_DOF=4). Five-wire feedback servos optional for production (KOSKALAK_SERVO_FEEDBACK).
- **Power:** 2S 7.4V 1000mAh LiPo + TP5100 charger + USB-C for prototype. Production target: 18350 Li-ion cells (UN 38.3 pre-certified). ADC battery monitoring included.
- **Connectivity:** WiFi 2.4GHz + BLE 5.0 (onboard)

BOM ~$105/unit at prototype volume (8-DOF). ~$85/unit at 4-DOF. See hardware repo for PCB schematics and 3D models.

### Compile-Time Configuration

| Define | Values | Default | Effect |
|--------|--------|---------|--------|
| `KOSKALAK_DOF` | 4, 8 | 8 | Number of servo channels (4-DOF = 1 per leg, 8-DOF = 2 per leg) |
| `KOSKALAK_BATTERY_CELLS` | 1, 2 | 2 | Battery chemistry (1S = 3.7V, 2S = 7.4V). Affects ADC divider ratio. |
| `KOSKALAK_SERVO_FEEDBACK` | defined/undefined | undefined | Enable ADC-based servo position readback (requires 5-wire servos) |

### KT2 Reference
The KT2 "Kungfu Turtle" (Kame-lineage consumer robot, $99 retail, 800% Kickstarter funded) validates our MCU choice (ESP32-S3 at 240MHz), battery estimates (~4hr on 800mAh), and form factor durability (94mm x 78mm x 33mm, 160g). Key adaptations from KT2:
- **4-DOF option** — KT2 likely uses 4 actuators (1 per leg), achieving dynamic gaits via momentum transfer + IMU feedback. We support both 4-DOF and 8-DOF via compile flag.
- **Five-wire feedback servos** — position feedback to MCU for true closed-loop joint control. Optional via KOSKALAK_SERVO_FEEDBACK.
- **Standalone balance controller** — extracted as independent FreeRTOS task (matches TurtleOS pattern). Application/gait code doesn't worry about balance.
- **Capacitive touch** — KT2 uses touch for interaction. We use ESP32-S3 native touch for push-to-talk, head pat, wake, and sleep gestures.
- **IMU gesture detection** — pickup, shake (call server), tap (wake), tip-over (safety crouch)
- **18350 Li-ion cells** (production) — safer, certifiable, user-replaceable. Must be UN 38.3 pre-certified.
- **Snap-fit modular chassis** (production) — 12-part KT2-inspired assembly, operator-serviceable

### Compliance Requirements

- **FCC:** Use pre-certified ESP32-S3-WROOM-1 module → only Subpart B (unintentional radiator) testing needed (~$3,000-5,000). Do NOT use bare ESP32-S3 chip.
- **Battery:** Source cells with existing UN 38.3 certification (transport safety). IEC 62133-2 for operational safety.
- **CE (EU):** Radio Equipment Directive (RED) 2014/53/EU — covered by pre-certified module for RF; host product needs EMC + safety testing.
