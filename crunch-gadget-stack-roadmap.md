# Crunch Gadget: Technical Stack & 6-Month Roadmap

**Date:** April 3, 2026
**Author:** Crunch Engineering
**Status:** Proposal Draft

---

## Executive Summary

Crunch Gadget is an intelligent, personality-driven tabletop robot for hospitality front-of-house operations. Built on the open-source miniKame quadruped platform (ESP8266, 8-DOF, 3D-printed PLA), it upgrades the brain to an ESP32-S3 and adds a display, microphone, and speaker to become a guest-facing assistant for bookings, ordering, waitlist management, and guest engagement — all connected to the existing Crunch back-of-house platform via WebSocket and REST APIs.

The form factor is the key differentiator: rather than another faceless tablet or kiosk, Crunch Gadget is a small, animated robot companion that sits on a host stand or table, moves expressively, and interacts with guests through voice and a small screen. Think "restaurant mascot that actually does useful work."

---

## 1. What We're Building On

### The Kame Platform (What We Get for Free)

The miniKame (github.com/bqlabs/miniKame) gives us a proven, 3D-printable quadruped chassis:

- **Frame:** Fully 3D-printed PLA body, legs, and joints (FreeCAD source files available)
- **Locomotion:** 4 legs with parallelogram mechanism, 8 servos (2 per leg), 8 degrees of freedom
- **Firmware:** C++ oscillator-based gait engine on Arduino framework
- **Joints:** Metal ball bearings for durability (not printed bushings)
- **License:** CC BY-SA (commercial use allowed with attribution)
- **Successor:** Kame32 exists with ESP32 upgrade path already documented

Since we already have a 3D printer, the mechanical platform is essentially free to prototype. The work is in upgrading the electronics, adding interaction hardware, and building the software layer that connects it to Crunch.

### The Crunch Platform (What Already Exists)

The Crunch backend already has everything a front-of-house gadget needs to consume:

- **Real-time events** via Socket.IO + Redis (order.created, order.updated, inventory changes)
- **Webhook subscriptions** for order, inventory, and member events
- **REST APIs** for menus, orders, locations, members, categories, modifiers
- **App Store integration pattern** with OAuth tokens for device authentication
- **MCP servers** (server-ops-mcp on port 8787, server-square-mcp on port 8788) for AI agent tool access
- **AI agents** via server-chatkit (FastAPI + OpenAI Agents SDK) — Crunchy and Alex already exist
- **Square POS sync** for payments, catalog, labor, and customer data
- **Multi-tenant isolation** with organizationId scoping on every endpoint

No new backend services need to be built from scratch. The gadget registers as an "app" in the Crunch App Store, subscribes to events, and uses existing APIs.

---

## 2. Hardware Stack

### Core Compute: ESP32-S3-WROOM-1 Module

**Why ESP32-S3 over the original ESP8266:**

| Spec | ESP8266 (Kame original) | ESP32-S3 (Crunch Gadget) |
|------|------------------------|--------------------------|
| CPU | Single-core 80MHz | Dual-core 240MHz LX7 |
| RAM | 80KB | 512KB SRAM + 8MB PSRAM (module) |
| Flash | 4MB | 16MB (octal SPI) |
| WiFi | 2.4GHz b/g/n | 2.4GHz b/g/n (40MHz BW) |
| Bluetooth | None | BLE 5.0 |
| AI | None | Vector instructions (ESP-NN) |
| GPIO | 17 | 45 programmable |
| USB | None | Native USB OTG |

The ESP32-S3's vector instructions enable on-device wake-word detection and basic keyword spotting, keeping the cloud dependency to actual conversation processing.

### Bill of Materials (Per Unit, Prototype Pricing)

| Component | Model / Spec | Qty | Unit Cost (USD) | Total |
|-----------|-------------|-----|-----------------|-------|
| **Compute** | ESP32-S3-DevKitC-1 (N16R8) | 1 | $10 | $10 |
| **Servos** | MG90S metal-gear micro servo | 8 | $4 | $32 |
| **Display** | 2.4" IPS LCD (ST7789, 240x320, SPI) | 1 | $8 | $8 |
| **Audio Out** | MAX98357A I2S amp + 3W speaker | 1 | $6 | $6 |
| **Audio In** | INMP441 I2S MEMS microphone | 1 | $3 | $3 |
| **Battery** | 2S 7.4V 1000mAh LiPo | 1 | $12 | $12 |
| **Power** | TP5100 2S LiPo charger + USB-C | 1 | $4 | $4 |
| **Servo Driver** | PCA9685 16-ch PWM (I2C) | 1 | $4 | $4 |
| **Bearings** | 3x7x3mm ball bearings (leg joints) | 16 | $0.30 | $5 |
| **3D Print Filament** | PLA per unit (~150g) | 1 | $4 | $4 |
| **PCB** | Custom carrier board (JLCPCB) | 1 | $5 | $5 |
| **Misc** | Wiring, connectors, fasteners, standoffs | 1 | $7 | $7 |
| | | | **Per-unit total** | **~$100** |

At production scale (500+ units), component costs drop roughly 30-40%, bringing the BOM to ~$60-65/unit.

### Optional Add-Ons (Phase 2+)

| Component | Purpose | Cost |
|-----------|---------|------|
| OV2640 camera module | Guest recognition, QR code scanning | $5 |
| NeoPixel LED ring (12 LEDs) | Status indicators, mood lighting | $4 |
| NFC reader (PN532) | Tap-to-pay / loyalty card | $6 |
| Capacitive touch pads | Head-pat interaction, gesture input | $2 |

---

## 3. Software Stack

### Layer 1: Firmware (On-Device — ESP32-S3)

**Framework:** ESP-IDF v5.x with Arduino component (best of both worlds — RTOS + Arduino library ecosystem)

**Key firmware modules:**

| Module | Purpose | Libraries / Approach |
|--------|---------|---------------------|
| **WiFi Manager** | Provisioning, reconnection, OTA | ESP-IDF WiFi + SmartConfig / BLE provisioning |
| **Motion Engine** | Gait control, expressive animations | Ported from miniKame C++ oscillator engine |
| **Display Driver** | UI rendering on 2.4" LCD | LVGL (Light and Versatile Graphics Library) |
| **Audio Pipeline** | Mic capture, speaker playback | ESP-ADF (Audio Development Framework) |
| **Wake Word** | On-device "Hey Crunchy" detection | ESP-SR (Speech Recognition) or Porcupine Lite |
| **WebSocket Client** | Real-time connection to Crunch | ESP-IDF WebSocket + JSON parser (cJSON) |
| **HTTP Client** | REST API calls to server-ops | ESP-IDF HTTP client |
| **OTA Updater** | Firmware updates over WiFi | ESP-IDF OTA with dual partition scheme |
| **State Machine** | Device modes (idle, listening, talking, walking) | Custom FreeRTOS task-based FSM |

**Language:** C/C++ (required for ESP-IDF / real-time servo control)

### Layer 2: Cloud Gateway (New Lightweight Service)

A thin gateway service that bridges the gadget's constrained protocol needs with the full Crunch API surface. This is intentionally minimal — it does not duplicate server-ops logic.

**Framework:** Node.js + Express (consistent with server-ops stack)
**Port:** 3004
**Responsibilities:**

- **Device registry & auth:** Issue and validate device tokens (JWT), tied to organizationId + locationId
- **Voice proxy:** Accept audio stream from gadget → forward to OpenAI Whisper API → get transcript → forward to server-chatkit agent → stream TTS response back
- **Event fan-out:** Subscribe to server-ops webhooks, filter by device's location, and push relevant events down the gadget's WebSocket
- **OTA management:** Track firmware versions per device, serve update binaries
- **Health & telemetry:** Collect device heartbeats, battery level, WiFi signal, error logs

This is the only new backend service. Everything else uses existing Crunch infrastructure.

### Layer 3: AI & Voice (Leveraging Existing Crunch AI)

The conversation flow:

```
Guest speaks → Gadget mic captures audio
    → Wake word detected on-device (ESP-SR)
    → Audio streamed to Cloud Gateway
    → Gateway forwards to OpenAI Whisper (speech-to-text)
    → Transcript sent to server-chatkit agent (Crunchy)
    → Agent uses MCP tools to check menu, availability, place orders
    → Response text returned to Gateway
    → Gateway calls OpenAI TTS API (text-to-speech)
    → Audio streamed back to gadget speaker
    → Gadget plays response + performs expressive animation
```

**No new AI model training needed.** The existing Crunchy agent in server-chatkit already has access to menus, orders, inventory, and locations via MCP tools. We just need a new "gadget persona" system prompt that's optimized for voice-length responses and front-of-house tasks.

### Layer 4: Companion App (Device Setup & Management)

**Framework:** React Native (shared codebase with potential future Crunch mobile app) OR simply a new section in the existing web-platform dashboard.

**Recommended approach:** Add a "Devices" section to web-platform (Next.js). This avoids a new app and keeps management in the existing dashboard operators already use.

**Features:**

- Device pairing via BLE / QR code WiFi provisioning
- Assign device to location + role (host stand, table, bar)
- Configure personality (voice, animation style, greeting)
- Monitor device health (battery, connectivity, uptime)
- Push firmware updates
- View conversation logs and guest interaction analytics

---

## 4. Integration Architecture

```
                                    ┌─────────────────────┐
                                    │   Crunch Gadget      │
                                    │   (ESP32-S3)         │
                                    │                      │
                                    │  ┌──────┐ ┌───────┐ │
                                    │  │ Mic  │ │Display│ │
                                    │  └──┬───┘ └───┬───┘ │
                                    │     │         │     │
                                    │  ┌──┴─────────┴──┐  │
                                    │  │  Firmware FSM  │  │
                                    │  │  + Motion Eng  │  │
                                    │  └───────┬───────┘  │
                                    └──────────┼──────────┘
                                               │ WiFi (WebSocket + HTTPS)
                                               │
                                    ┌──────────┴──────────┐
                                    │  server-gadget       │
                                    │  (Gateway, port 3004)│
                                    │                      │
                                    │  Device Auth         │
                                    │  Voice Proxy         │
                                    │  Event Fan-out       │
                                    │  OTA Management      │
                                    └──┬──────┬──────┬────┘
                                       │      │      │
                          ┌────────────┘      │      └────────────┐
                          │                   │                   │
                ┌─────────┴────────┐  ┌──────┴───────┐  ┌───────┴───────┐
                │  server-chatkit  │  │  server-ops   │  │  OpenAI APIs  │
                │  (AI Agents)     │  │  (Core API)   │  │  Whisper, TTS │
                │  port 8001       │  │  port 3000    │  │               │
                └─────────┬────────┘  └──────┬───────┘  └───────────────┘
                          │                  │
                ┌─────────┴────────┐         │
                │  MCP Servers     │         │
                │  ops-mcp: 8787   │    ┌────┴────┐
                │  square-mcp: 8788│    │ MongoDB │
                └──────────────────┘    └─────────┘
```

### Authentication Flow

1. Operator adds a new device in web-platform dashboard
2. Dashboard generates a device provisioning QR code (contains: org ID, location ID, one-time token)
3. Gadget scans QR via companion setup flow (phone camera → BLE to gadget)
4. Gadget exchanges one-time token for a long-lived device JWT via server-gadget
5. All subsequent API calls use this JWT, scoped to organizationId + locationId

### Event Flow (Real-Time Order Updates)

1. Guest places order via POS, app, or another gadget
2. server-ops emits `sales_order.created` webhook
3. server-gadget receives webhook, matches to subscribed devices at that location
4. server-gadget pushes event down device WebSocket
5. Gadget displays order status on LCD, plays acknowledgment animation

---

## 5. Front-of-House Use Cases (MVP)

| Use Case | How It Works | APIs Used |
|----------|-------------|-----------|
| **Greet & seat guests** | Guest approaches host stand → gadget greets, asks party size → checks availability → quotes wait time | Voice pipeline + custom reservation logic |
| **Take orders at table** | Guest speaks order → agent maps to menu items + modifiers → confirms → creates sales order | `POST /api/v1/sales-orders`, `GET /api/v1/items`, `GET /api/v1/modifier-lists` |
| **Answer menu questions** | "What's gluten-free?" → agent queries items by dietary tags | `GET /api/v1/items` with filters, `GET /api/v1/categories` |
| **Order status** | "Where's my food?" → agent looks up order by table → reports status | `GET /api/v1/sales-orders/{orderNumber}` |
| **Waitlist management** | Display queue position, estimated wait, notify when table ready | WebSocket events + custom waitlist state |
| **Daily specials** | Animate and announce specials, show images on display | `GET /api/v1/menus` + display rendering |
| **Call server** | "Can I get my check?" → notifies staff via real-time event | WebSocket emit to staff devices |

---

## 6. Six-Month Roadmap

### Month 1: Hardware Prototype & Firmware Foundation

**Goal:** A working Kame robot with ESP32-S3 that connects to WiFi and talks to Crunch.

**Hardware tasks:**
- Fork miniKame, adapt chassis for ESP32-S3 DevKit + display + speaker + mic mounting
- Design and order custom carrier PCB (PCA9685 servo driver + audio amps + power management)
- Print first prototype chassis, assemble with upgraded electronics
- Test servo control and gait engine on ESP32-S3

**Software tasks:**
- Port miniKame oscillator engine from ESP8266 → ESP-IDF (ESP32-S3)
- Implement WiFi provisioning (SmartConfig or BLE)
- Basic WebSocket client connecting to server-ops
- Display "Hello World" on 2.4" LCD via LVGL
- Audio capture test (INMP441 → I2S → WAV buffer)

**Deliverable:** Robot walks, connects to WiFi, displays text, records audio. Not yet useful, but proving all hardware works together.

**Estimated CapEx:** ~$500 (5 prototype units + PCB order + tools)

---

### Month 2: Voice Pipeline & AI Integration

**Goal:** Speak to the gadget, get an intelligent response from Crunchy.

**Software tasks:**
- Implement wake-word detection using ESP-SR ("Hey Crunchy")
- Build server-gadget gateway service (Node.js, port 3004):
  - Device registration and JWT auth
  - Audio stream endpoint (WebSocket binary frames)
  - Whisper STT integration
  - server-chatkit agent forwarding
  - OpenAI TTS response streaming
- Create "Gadget Persona" system prompt for server-chatkit:
  - Short, voice-optimized responses (under 30 words)
  - Front-of-house context (menus, orders, greetings)
  - Personality traits (friendly, helpful, slightly playful)
- Firmware audio pipeline: mic → wake word → stream to gateway → receive TTS → play on speaker
- Basic expressive animations tied to conversation state (listening pose, thinking wiggle, talking bounce)

**Deliverable:** Say "Hey Crunchy, what's on the menu?" and get a spoken response with the gadget animating while it talks.

**Estimated CapEx:** ~$200 (OpenAI API credits for dev/testing)

---

### Month 3: Order Flow & Display UI

**Goal:** Complete order-taking flow, useful display UI, real-time event handling.

**Software tasks:**
- LVGL display UI:
  - Idle screen (animated Crunch branding + time + location name)
  - Conversation screen (transcript + visual feedback)
  - Order confirmation screen (line items, modifiers, total)
  - Queue / waitlist display
- Order-taking agent flow:
  - Parse voice orders into structured line items
  - Handle modifiers ("no onions", "extra cheese")
  - Confirmation step before submitting
  - `POST /api/v1/sales-orders` integration
- Real-time event subscription:
  - server-gadget subscribes to org webhooks
  - Fan-out to location-specific devices
  - Gadget receives and displays order status changes
- Staff notification system (gadget → WebSocket → staff dashboard/devices)

**Deliverable:** Full order-taking demo: guest speaks order → gadget confirms on screen → order appears in Crunch dashboard and Square POS.

**Estimated CapEx:** ~$300 (additional prototypes, API credits)

---

### Month 4: Device Management & Hardening

**Goal:** Production-grade device management, OTA updates, multi-device support.

**Software tasks:**
- web-platform "Devices" dashboard section:
  - Device pairing flow (QR provisioning)
  - Device list with health status (online, battery, signal, firmware version)
  - Location assignment and role configuration
  - Personality / voice configuration
  - Conversation log viewer
- OTA firmware update system:
  - Dual-partition A/B update scheme on ESP32-S3
  - server-gadget serves firmware binaries
  - Dashboard triggers updates per-device or fleet-wide
  - Automatic rollback on boot failure
- Error handling and resilience:
  - Graceful offline mode (display cached menu, queue requests)
  - Auto-reconnect with exponential backoff
  - Watchdog timer and automatic reboot on firmware hang
  - Telemetry collection (crash logs, latency metrics, battery cycles)

**Hardware tasks:**
- Iterate chassis design based on prototype learnings (cable management, serviceability, ventilation)
- Design charging dock (USB-C cradle for overnight charging)

**Deliverable:** Fleet of 5+ gadgets manageable from one dashboard, with OTA updates working.

**Estimated CapEx:** ~$800 (10 units for field testing + charging docks + PCB revision)

---

### Month 5: Pilot Testing & Iteration

**Goal:** Deploy at 1-2 real restaurant locations, gather feedback, iterate.

**Tasks:**
- Deploy 3-5 gadgets at pilot restaurant(s)
- On-site setup and staff training
- Collect metrics: order accuracy, guest engagement, voice recognition success rate, uptime
- Iterate on:
  - Voice prompt tuning (noisy restaurant environments)
  - Animation library (expand expressive behaviors)
  - Display UI refinements based on real usage
  - Menu question handling accuracy
- Acoustic hardening:
  - Noise cancellation (ESP-ADF AEC — Acoustic Echo Cancellation)
  - Beamforming if using dual-mic config
  - Adjust wake-word sensitivity for ambient noise
- Security audit:
  - Device token rotation
  - API rate limiting per device
  - Encrypted firmware updates (ESP32-S3 AES-XTS flash encryption)

**Deliverable:** Pilot report with metrics, guest feedback, and iteration backlog.

**Estimated CapEx:** ~$1,500 (pilot units, on-site costs, travel)

---

### Month 6: Production Prep & Launch Readiness

**Goal:** Finalize hardware design, prepare for small-batch manufacturing, launch plan.

**Tasks:**
- Final chassis design for injection-mold evaluation (or optimized FDM for initial batches)
- Production PCB design (integrated board, not dev kit):
  - ESP32-S3-WROOM-1 module soldered directly
  - Integrated servo driver, audio, power management
  - FCC/CE pre-compliance testing
- Manufacturing documentation:
  - Assembly guide
  - Quality control checklist
  - Firmware flashing procedure
- Packaging design and unboxing experience
- Operator onboarding flow (app → pair → configure → go live)
- Pricing model finalization:
  - Hardware cost + margin
  - Monthly SaaS fee (bundled with Crunch subscription or add-on)
- Documentation and support materials
- Marketing assets (demo videos, landing page)

**Deliverable:** Production-ready design files, 20-unit pilot batch, go-to-market plan.

**Estimated CapEx:** ~$3,000 (production PCB tooling, FCC pre-scan, pilot batch)

---

## 7. Total CapEx Summary (6 Months)

| Category | Amount (USD) |
|----------|-------------|
| Prototype hardware (components, PCBs, 3D printing) | $1,500 |
| Production pilot batch (20 units @ ~$80/unit) | $1,600 |
| PCB design & tooling (custom production board) | $1,000 |
| API costs (OpenAI Whisper + TTS + GPT for dev/testing) | $800 |
| FCC/CE pre-compliance testing | $1,500 |
| Pilot deployment (installation, travel) | $1,000 |
| Development tools & licenses | $500 |
| **Total CapEx** | **~$7,900** |

This excludes developer salaries. The software work is roughly 2 full-time engineers for 6 months (1 firmware/embedded, 1 full-stack for gateway + dashboard).

---

## 8. Technology Choices: Why These and Not Others

### Why ESP32-S3 and not Raspberry Pi?

A Pi would make software easier (run Python, full Linux) but kills the product economics. An ESP32-S3 module costs $4 at scale vs. $35+ for a Pi. Battery life is hours vs. minutes. Boot time is under 2 seconds vs. 30+. For a device that needs to run all day on a restaurant floor, the constraints of an MCU are worth the firmware complexity.

### Why not build our own voice model?

OpenAI Whisper + TTS are best-in-class, already integrated into our ChatKit stack, and cost fractions of a cent per interaction. Training a custom model would take months and deliver worse results. The on-device wake-word (ESP-SR) keeps the cloud dependency to actual conversations only.

### Why keep the Kame quadruped form factor?

The walking robot form is the product moat. Any competitor can put a screen on a table. Nobody else has a friendly little robot that walks around, does a happy dance when you order, and wiggles while thinking about your question. The personality IS the product. The hospitality industry sells experiences, and this is an experience.

### Why a thin gateway instead of connecting directly to server-ops?

The ESP32 has limited TLS memory and can't handle complex OAuth flows or audio processing. The gateway is a lightweight translator: it handles the heavy crypto, audio transcoding, and event filtering that would be impractical on the MCU. It also gives us a single place to manage device fleet concerns (OTA, telemetry, auth) without polluting server-ops.

---

## 9. Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Restaurant noise degrades voice recognition | High | Acoustic echo cancellation (ESP-ADF), directional mic, push-to-talk fallback, display-based ordering backup |
| WiFi reliability in commercial kitchens | Medium | Auto-reconnect, offline cache, graceful degradation (display-only mode) |
| Servo wear in continuous operation | Medium | Metal-gear servos, idle mode when not interacting, modular leg replacement design |
| Battery life insufficient for full shift | Low | USB-C charging dock at host stand, 1000mAh should cover 6-8h with sleep modes |
| Regulatory compliance (FCC/CE) delays | Medium | Start pre-compliance in Month 5, use pre-certified ESP32-S3 module (reduces scope) |
| Guest discomfort with robot interaction | Medium | Always offer human staff fallback, optional "silent mode" (display-only), pilot feedback loop |

---

## 10. What We Need to Buy / Set Up (Not Already in Place)

### Hardware (Beyond the 3D Printer We Already Have)

- ESP32-S3 development kits (5x for prototyping)
- Servo tester / oscilloscope for debugging
- Soldering station (if not already available)
- JLCPCB account for PCB orders
- LiPo battery charger and storage

### Software & Services

- ESP-IDF toolchain (free, open-source)
- LVGL graphics library (free, MIT license)
- OpenAI API account (Whisper + TTS + GPT — usage-based billing)
- JLCPCB / EasyEDA for PCB design (free tier sufficient)
- FreeCAD or Fusion 360 for chassis modifications (FreeCAD is free)

### Team

- 1 Embedded/firmware engineer (ESP-IDF, C/C++, hardware bring-up)
- 1 Full-stack engineer (Node.js gateway + Next.js dashboard — can be existing Crunch team)
- Part-time: industrial / mechanical design for chassis iteration
- Part-time: existing Crunch AI team for agent persona tuning

---

## Appendix: Key References

- miniKame source: github.com/bqlabs/miniKame (CC BY-SA)
- Kame32 (ESP32 upgrade): github.com/bqlabs/kame32
- ESP-IDF documentation: docs.espressif.com/projects/esp-idf
- ESP-SR (speech recognition): github.com/espressif/esp-sr
- ESP-ADF (audio framework): github.com/espressif/esp-adf
- LVGL graphics: lvgl.io
- Crunch server-ops API: localhost:3000/api/v1/
- Crunch server-chatkit: localhost:8001
- Crunch server-ops-mcp: localhost:8787
- Crunch server-square-mcp: localhost:8788
