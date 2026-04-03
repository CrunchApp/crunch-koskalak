# Crunch Koskalak

Software stack for **Crunch Gadget** — a personality-driven tabletop robot for restaurant front-of-house operations.

Crunch Gadget sits on a host stand or table, greets guests, takes orders, answers menu questions, manages waitlists, and connects everything to the [Crunch](https://crunch.app) back-of-house platform. The form factor is a 3D-printed quadruped robot based on the [miniKame](https://github.com/bqlabs/miniKame) open-source platform, upgraded with a display, microphone, speaker, and cloud-connected AI.

## What's in This Repo

| Directory | Language | Purpose |
|-----------|----------|---------|
| `firmware/` | C/C++ (ESP-IDF) | Device firmware for ESP32-S3 — servo control, audio pipeline, display UI, networking |
| `server-gadget/` | TypeScript (Node.js/Express) | Cloud gateway — device auth, voice proxy, event fan-out, OTA management |
| `agent/` | Python | Sage agent definition — front-of-house AI persona for server-chatkit integration |

Hardware design (3D models, PCB schematics) is maintained in a separate repository.

## Architecture

```
                         ┌──────────────────────────┐
                         │    Crunch Gadget          │
                         │    (ESP32-S3)             │
                         │                           │
                         │  Mic ─► Wake Word (local) │
                         │  Speaker ◄─ I2S playback  │
                         │  2.4" LCD ◄─ LVGL UI      │
                         │  8 Servos ◄─ Gait engine  │
                         └─────────┬─────────────────┘
                                   │
                    ┌──────────────┼──────────────┐
                    │ MQTT (always-on)  WebSocket  │
                    │ telemetry, OTA    (voice     │
                    │ events, config    sessions)  │
                    └──────────────┼──────────────┘
                                   │
                         ┌─────────┴──────────┐
                         │   server-gadget     │
                         │   (port 3004)       │
                         │                     │
                         │  Device Registry    │
                         │  Voice Proxy        │
                         │  Event Fan-out      │
                         │  OTA Management     │
                         └──┬──────┬───────┬──┘
                            │      │       │
               ┌────────────┘      │       └────────────┐
               │                   │                     │
    ┌──────────┴────────┐  ┌──────┴───────┐  ┌─────────┴───────┐
    │  server-chatkit   │  │  server-ops   │  │  OpenAI APIs    │
    │  (Sage Agent)     │  │  (Core API)   │  │  Whisper + TTS  │
    │  port 8001        │  │  port 3000    │  │                 │
    └──────────┬────────┘  └──────┬───────┘  └─────────────────┘
               │                  │
    ┌──────────┴────────┐        │
    │  MCP Servers       │   ┌───┴─────┐
    │  ops-mcp: 8787     │   │ MongoDB │
    │  square-mcp: 8788  │   └─────────┘
    └────────────────────┘
```

### Communication Protocol Design

**MQTT** (persistent, always-on):
- Device heartbeat and telemetry (battery, WiFi signal, uptime)
- OTA update triggers and progress
- Event delivery from Crunch (order status changes, inventory alerts)
- Device configuration push
- Topic structure: `crunch/{orgId}/{locationId}/{deviceId}/{channel}`

**WebSocket** (on-demand, voice sessions only):
- Opened when wake word is detected, closed when conversation ends
- Binary audio frames (mic → server, TTS → device)
- Minimises idle RAM usage on the ESP32

### Voice Pipeline

```
1. Guest speaks
2. ESP-SR wake word detection on-device ("Hey Crunchy")
3. WebSocket opened to server-gadget
4. Audio streamed as PCM16 frames
5. server-gadget → OpenAI Whisper API (speech-to-text)
6. Transcript → server-chatkit Sage agent
7. Sage queries Crunch data via MCP tools
8. Response text → OpenAI TTS API (text-to-speech)
9. Audio streamed back to device via WebSocket
10. Device plays audio + expressive servo animation
11. WebSocket closed after conversation timeout
```

**Upgrade path:** Replace steps 5-8 with OpenAI Realtime API for streaming voice-to-voice with lower latency. The gateway proxy pattern supports both approaches.

### Sage Agent (Front-of-House AI)

Sage is a new agent persona in server-chatkit, purpose-built for voice interaction:

- **Voice-first:** Responses capped at ~30 words. No markdown, no lists, no URLs.
- **Read + Create only:** Can read menus, inventory, availability. Can create sales orders. Cannot modify or delete any back-of-house data.
- **Location-scoped:** Every query is bound to the device's assigned location.
- **Personality:** Warm, concise, slightly playful. Think friendly host, not chatbot.
- **MCP access:** server-ops-mcp (menus, orders, inventory) + server-square-mcp (payments, customers).
- **Guardrails:** No access to `business_context` aggregations, member management, or purchase orders. Front-of-house only.

## Device Authentication Flow

```
1. Operator adds device in web-platform → "Devices" dashboard
2. Dashboard generates QR code (orgId, locationId, one-time provisioning token)
3. Operator scans QR with phone → BLE transfers WiFi creds + token to gadget
4. Gadget exchanges one-time token for long-lived device JWT via server-gadget
5. JWT scoped to orgId + locationId, used for all subsequent API calls
6. Token rotation handled via MQTT command channel
```

## Front-of-House Use Cases (MVP)

| Use Case | Interaction | Crunch APIs |
|----------|-------------|-------------|
| **Greet guests** | Guest approaches → gadget greets, asks party size | Voice + proximity detection |
| **Take orders** | Voice order → confirm on screen → create sales order | `POST /sales-orders`, `GET /items`, `GET /modifier-lists` |
| **Menu questions** | "What's gluten-free?" → agent queries items | `GET /items` with filters, `GET /categories` |
| **Order status** | "Where's my food?" → look up by table/order number | `GET /sales-orders/{id}` |
| **Daily specials** | Display specials on LCD, announce via voice | `GET /menus` |
| **Call server** | "Can I get the bill?" → notify staff | WebSocket event to staff devices |
| **Waitlist** | Display queue position, notify when table ready | Custom state in server-gadget |

## Technology Decisions

### 4-DOF vs 8-DOF: Prototype Both, Let Pilots Decide

KT2 deep research revealed it likely uses 4 actuators (1 per leg), not 8. This halves servo cost and power draw, but requires momentum-based gaits that depend heavily on IMU feedback. Our firmware supports both via `KOSKALAK_DOF` compile flag. Default is 8-DOF (proven miniKame platform). Build a 4-DOF variant in Month 2 and let pilot restaurants choose.

### Why ESP32-S3 Only (No Raspberry Pi)

| Factor | ESP32-S3 | Raspberry Pi 5 |
|--------|----------|----------------|
| Module cost | ~$4 at scale | ~$35+ |
| Boot time | <2 seconds | 30+ seconds |
| Battery life | 6-8 hours (with sleep) | <2 hours |
| Idle power | ~30mA | ~600mA |
| Per-unit BOM | ~$100 | ~$495 |

The AI processing happens in the cloud. The device only needs to be a good I/O terminal — mic, speaker, display, servos. An MCU does this at 1/5 the cost with 10x the battery life.

### Why MQTT + WebSocket Hybrid (Not WebSocket-Only)

- MQTT persistent connection: ~10-20KB RAM. WebSocket persistent: ~40-80KB RAM.
- MQTT has QoS levels and persistent sessions — survives WiFi reconnects cleanly.
- WebSocket is better for streaming binary audio — low framing overhead.
- Hybrid: MQTT always-on for telemetry/events, WebSocket on-demand for voice only.

### Why a Separate Gateway (Not Direct to server-ops)

- ESP32 has limited TLS memory and can't handle complex OAuth flows.
- Audio transcoding (PCM16 → Whisper format) is impractical on MCU.
- Fleet concerns (OTA, telemetry, device auth) shouldn't pollute server-ops.
- Single service to manage device-specific protocol translation.

### Why a Separate Agent (Sage) Not a Crunchy Prompt Variant

- Different response length constraints (30 words vs unlimited).
- Different capability set (read + create only, no destructive ops).
- Different failure domain — a Sage crash shouldn't affect back-of-house Crunchy.
- Different context model — location-scoped, shift-aware, voice-first.

## Stack Summary

| Layer | Technology | Notes |
|-------|-----------|-------|
| **Device MCU** | ESP32-S3-WROOM-1 (N16R8) | Dual-core 240MHz, 8MB PSRAM, 16MB flash. **Pre-certified module required.** |
| **Device RTOS** | ESP-IDF v5.x (FreeRTOS) | Native framework, required for ESP-ADF/ESP-SR |
| **DOF config** | 8-DOF (default) or 4-DOF | Compile-time KOSKALAK_DOF flag. 8=miniKame, 4=KT2-style. |
| **Wake word** | ESP-SR (WakeNet) | On-device, ~80% accuracy in noise. Touch-to-talk as fallback. |
| **Audio pipeline** | ESP-ADF | I2S mic capture, echo cancellation, speaker playback |
| **Display UI** | LVGL 9.x | Embedded graphics on 2.4" SPI LCD (ST7789) |
| **IMU** | MPU-6050 6-axis (I2C) | Orientation, gesture detection (pickup/shake/tap/tip-over) |
| **Balance** | Standalone PD controller | Own FreeRTOS task, 50Hz. Invisible to gait code (TurtleOS pattern). |
| **Touch** | ESP32-S3 native capacitive | 4 zones, push-to-talk fallback, no external IC |
| **LEDs** | WS2812 NeoPixel via RMT | 8-LED ring, state-mapped patterns (breathe, chase, flash, rainbow) |
| **Battery** | ADC monitor (1S/2S config) | Voltage, percentage, low/critical alerts, MQTT telemetry |
| **Servo control** | PCA9685 via I2C | Balance offsets applied to gait oscillator. Optional feedback servos. |
| **Device → Cloud** | MQTT (persistent) + WebSocket (voice) | Hybrid for optimal RAM/battery |
| **Gateway** | Node.js + Express + TypeScript | Thin proxy — port 3004 |
| **Voice STT** | OpenAI Whisper API | Via gateway proxy. Realtime API as upgrade path. |
| **Voice TTS** | OpenAI TTS API | Via gateway proxy |
| **AI Agent** | Sage (OpenAI Agents SDK, Python) | In server-chatkit alongside Crunchy/Alex |
| **Data access** | MCP (server-ops-mcp, server-square-mcp) | Streamable HTTP transport |
| **Fleet monitoring** | Memfault | Crash reporting, device health, OTA analytics |
| **OTA updates** | ESP-IDF native (dual A/B partitions) | Served from server-gadget. BLE-triggered WiFi OTA preferred. |
| **Device management** | web-platform dashboard section | New "Devices" page in existing Next.js app |
| **Testing** | Vitest (gateway), idf.py host tests (firmware) | |

## CAPEX Estimate (6 Months)

| Category | Amount (USD) |
|----------|-------------|
| Prototype hardware (5 units, both 4-DOF and 8-DOF variants) | $1,500 |
| Pilot batch (20 units @ ~$85/unit) | $1,700 |
| PCB design and tooling | $1,000 |
| OpenAI API costs (dev + testing) | $800 |
| FCC Subpart B pre-compliance (pre-certified module) | $3,500 |
| Pilot deployment (installation, travel) | $1,000 |
| Dev tools and licences | $500 |
| Memfault subscription (dev tier) | $0 (free tier) |
| **Total CAPEX (excl. salaries)** | **~$10,000** |

**CAPEX increase vs original:** +$2,100, driven by more realistic FCC testing budget ($3,500 vs $1,500) and 4-DOF prototype variant. This is conservative — could be lower if FCC pre-scan passes cleanly.

**Team:** 1 embedded/firmware engineer + 1 full-stack engineer (gateway + dashboard). Part-time: AI team for Sage agent tuning, mechanical designer for chassis iteration (in hardware repo).

## 6-Month Roadmap

| Month | Focus | Deliverable |
|-------|-------|-------------|
| **1** | Hardware prototype + firmware foundation | Walking 8-DOF robot, WiFi connected, displays text, captures audio. **Source UN 38.3 pre-certified battery cells.** |
| **2** | Voice pipeline + Crunch integration + 4-DOF variant | "Hey Crunchy, what's on the menu?" → spoken response. **Build and test 4-DOF prototype alongside 8-DOF.** |
| **3** | Order flow + LVGL display UI | Full ordering demo: voice → confirm on screen → order in Crunch + Square POS. Touch-to-talk fallback. |
| **4** | Device management + hardening | Fleet of 5+ gadgets managed from dashboard, OTA updates, offline resilience. **Begin FCC pre-compliance.** |
| **5** | Pilot deployment + iteration | 3-5 devices at real restaurant(s), metrics collection, acoustic tuning. Determine 4-DOF vs 8-DOF preference. |
| **6** | Production prep + launch readiness | Final PCB (pre-certified WROOM module), 20-unit batch, go-to-market materials |

## Getting Started

### Prerequisites
- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/) toolchain
- Node.js 20+
- MQTT broker (e.g., `brew install mosquitto` for local dev)
- Running Crunch platform (server-ops, server-chatkit, MCP servers)

### Quick Start
```bash
# 1. Start Crunch platform (from Crunch root)
npm run dev

# 2. Start MQTT broker
mosquitto -c /usr/local/etc/mosquitto/mosquitto.conf

# 3. Start gadget gateway
cd crunch-koskalak/server-gadget
cp .env.example .env  # configure your keys
npm install && npm run dev

# 4. Build and flash firmware (with device connected)
cd crunch-koskalak/firmware
idf.py set-target esp32s3
idf.py build flash monitor
```

## Licence

Proprietary. Internal Crunch use only.

Chassis design derived from [miniKame](https://github.com/bqlabs/miniKame) (CC BY-SA 4.0) — attribution maintained in hardware repo.
