# KT2 Kungfu Turtle Architecture and Stack Deep-Dive

## Executive summary

KT2 “Kungfu Turtle” is publicly described as a pocket-sized, DIY-assembled, four-legged desktop robot companion and “fighter bot,” positioned as both an entertainment device (notably “99 built-in games”) and a simple coding/education platform (drag-and-drop block coding). citeturn22view0turn13view0turn17view0turn27view0

Across multiple independent write-ups that appear to quote or summarize the campaign materials, the most consistently documented physical specifications are **94 × 78 × 33 mm** and **160 g**, with **USB‑C charging** and an **~800 mAh** internal battery. citeturn27view0turn28view0turn29view0

The compute/control core is widely reported as an **ESP32‑S3 (240 MHz class)** MCU platform with onboard **Wi‑Fi (2.4 GHz) and Bluetooth (BLE)**; user interaction is described as including a **touch surface/panel** and an onboard **microphone** (voice control/commands), plus a **6‑axis IMU** for motion/interaction sensing. citeturn4view0turn25search24turn27view0turn28view0turn17view0

Software is branded as **“TurtleOS”**, and multiple sources state the device can receive **firmware updates over Bluetooth or Wi‑Fi**. citeturn27view0turn28view0turn29view0

A privacy-oriented claim attributed to the founder emphasizes minimalist sensing: **no integrated camera and no screen**, with interaction via actions and lighting instead. citeturn20search16

From an engineering standpoint, the public record is *uneven*: while high-level specs and feature claims are plentiful, there are **no publicly located official schematics, PCB layouts, FCC filings for the end product, detailed actuator part numbers, or a formal developer SDK/API** in the sources accessible here. (The Kickstarter project page itself was not reliably retrievable in this environment; the report therefore triangulates from accessible official pages and multiple secondary reports that cite the campaign.) citeturn22view0turn27view0turn28view0turn14view0

## Source base and verified specifications

Primary/near-primary sources used here include the publicly accessible KT2 landing/FAQ on the official site and a product mention in a mainstream magazine PDF. citeturn22view0turn19view0 Secondary sources (tech press and gadget sites) are used as corroboration and to recover likely Kickstarter-sourced details when direct Kickstarter access is limited. citeturn27view0turn28view0turn17view0turn4view0turn14view0

### Confirmed, publicly stated attributes

The table below distinguishes **documented facts** (supported by direct quotes/figures in accessible sources) from **inferred** points.

| Area | Attribute | Publicly stated value | Evidence | Confidence |
|---|---|---:|---|---|
| Mechanics | Form factor | Small four-legged “desktop” robot; legs are individually controlled/jointed | citeturn17view0turn19view0turn13view0 | High |
| Mechanics | “Joint count” language | Repeatedly described as a “4‑joint” action robot | citeturn13view0turn19view0 | High (as phrased), Medium (interpretation) |
| Dimensions | Size | 94 × 78 × 33 mm | citeturn27view0turn28view0turn29view0 | High |
| Mass | Weight | 160 g | citeturn27view0turn28view0turn29view0 | High |
| Power | Battery capacity | 800 mAh | citeturn27view0turn28view0turn29view0 | High |
| Power | Reported runtime | “Up to 4 hours” (also “4–6 hours depending on usage” on the official FAQ page) | citeturn27view0turn28view0turn22view0 | High for “multi-hour”; Medium for exact figure |
| Power | Charging connector | USB Type‑C | citeturn27view0turn28view0turn29view0 | High |
| Wireless | Connectivity | Wi‑Fi + Bluetooth | citeturn27view0turn28view0turn17view0turn15search12 | High |
| Compute | MCU family | ESP32‑S3 class; “240 MHz” appears in multiple summaries | citeturn4view0turn25search24 | Medium-High |
| Sensors | IMU | “Six-axis IMU” mentioned in multiple sources | citeturn4view0turn17view0turn25search24 | Medium-High |
| Sensors | Touch | Touch sensor/panel for interaction | citeturn27view0turn28view0turn17view0 | High |
| Sensors | Microphone | Microphone for voice commands/interaction | citeturn27view0turn28view0turn25search24 | High |
| Software | OS/firmware name | “TurtleOS” | citeturn19view0turn17view0turn27view0turn28view0 | High |
| Updates | Firmware updates | OTA updates via Bluetooth or Wi‑Fi are explicitly described | citeturn28view0turn27view0turn29view0 | High |
| Content | Built-in games | 99 built‑in games | citeturn22view0turn13view0turn17view0turn27view0 | High |
| “DIY” | Assembly framing | “12 parts” and “20 steps” style assembly repeated widely | citeturn22view0turn19view0turn13view0turn27view0 | High |
| Privacy/UI | Camera/screen | Claimed “no integrated cameras or screen” (founder quote) | citeturn20search16 | Medium (single-source quote) |
| Business model | Subscription | Official FAQ says no subscription; updates free for backers | citeturn22view0 | High |

### What the public record does **not** currently contain (important gaps)

No accessible public sources in this review provide: **full actuator specs**, **gearbox ratios**, **motor/servo part numbers**, **PCB photos/teardowns**, **connector pinouts**, **a published developer API**, **mobile app package identifiers**, **cloud endpoints**, or **formal compliance certificates for the assembled product (FCC/CE test reports)**. The campaign tracker does list “open source case file” and a “motor upgrade” note, but does not expose the underlying documents in an accessible form here. citeturn14view0turn23search0

## Mechanical system and motion control

### Observed external architecture

Public imagery and descriptions consistently portray KT2 as a compact “power-bank-like” core body with four short legs. The legs are described as jointed and controllable individually, enabling not just walking, but dynamic stunts (somersaults, rapid self-righting, “one-handed pushups”). citeturn17view0turn13view0turn4view0

The motion stack is explicitly positioned as a key differentiator: an “entire motion control system” is claimed to be designed specifically for KT2, paired with TurtleOS to “unlock the full potential” of the robot. citeturn13view0turn17view0turn19view0

image_group{"layout":"carousel","aspect_ratio":"1:1","query":["KT2 Kungfu Turtle robot","KT2 Kungfu Turtle four legged robot on desk","KT2 Kungfu Turtle DIY assembly robot","KT2 Kungfu Turtle robot close up legs"],"num_per_query":1}

### Interpreting “4‑joint” in engineering terms

**Confirmed fact:** marketing repeatedly calls the device a “4‑joint” robot. citeturn13view0turn19view0

**Speculative interpretation (Confidence: Medium):** “4‑joint” most plausibly refers to **4 DoF total**, i.e., **one actuated joint per leg** (hip pitch), rather than multi-DoF legs. This interpretation is consistent with (a) the extreme compactness (94×78×33 mm), (b) the visual simplicity of the legs, and (c) the emphasis that “if you ever designed a 4‑joint doglike robot, you know how hard it is to move it.” citeturn13view0turn27view0turn19view0

If KT2 truly has only four actuators, then “kungfu moves” and self-righting likely rely on a **highly dynamic gait library**, **precise inertial feedback**, and **planned momentum transfer**, more like a “minimal-DoF dynamic toy” than a statically stable quadruped. This fits the strong emphasis on the motion control system plus IMU sensing. citeturn13view0turn17view0turn4view0

### Actuators, joints, and gearboxes

**Confirmed fact:** one campaign update headline (via a third-party tracker) references a “motor upgrade” unlocking “all-new five-wire servos.” citeturn14view0turn23search0

**Speculative reconstruction (Confidence: Medium):**
- “Five-wire servo” likely indicates a **feedback-capable servo** variant (e.g., extra wires beyond power/signal for encoder/pot feedback or a bus/aux line). The campaign language implies upgraded flexibility, consistent with higher-performance or feedback servos. citeturn14view0turn23search0  
- Given the size/weight class, the actuators are likely **micro servos or custom micro gearmotors with position feedback**, integrated inside the main body with short lever arms to the legs (to maintain a low profile).  
- Gearboxes are expected to be **high reduction** (plastic or metal micro-gears) to achieve “stunt” torque in a small envelope, at the cost of audible gear noise—common in micro-actuated toys.

### Materials, enclosure, and assembly approach

**Confirmed facts:**
- KT2 is marketed as **DIY assembly** with “12 parts” and “20 steps,” suggesting a design optimized for **few large subassemblies**, **minimal fasteners**, and **connectorized internals** rather than intricate build. citeturn22view0turn19view0turn13view0turn27view0
- “Modular accessories” and customization (stickers, 3D-printed eyes) are explicitly encouraged. citeturn13view0turn17view0

**Speculative reconstruction (Confidence: Medium):** the enclosure is most plausibly:
- an **injection-molded** top cover (PC/ABS typical for consumer electronics),  
- a **lower structural frame** that captures actuators and batteries,  
- legs in injection-molded polymer (nylon/PA or ABS), with metal pins/screws at pivots.

## Electronics, sensing, and power system

### Compute platform

Multiple sources identify KT2’s “programmable chipset” as centered on an **ESP32‑S3** class MCU, often noting “240 MHz.” citeturn4view0turn25search24turn20search16 The ESP32‑S3 family is a dual-core MCU platform with integrated 2.4 GHz Wi‑Fi and Bluetooth LE. citeturn36search5

**Speculation (Confidence: Medium):** KT2 likely uses an **ESP32‑S3 module** (rather than a bare chip) to simplify RF design, assembly yield, and regulatory reuse. This is common in small connected devices and aligns with the project’s consumer scale. Support documentation for ESP32-S3 modules is extensive and designed for these integration patterns. citeturn36search13turn36search17

### Sensors and interaction I/O

**Confirmed user-facing I/O and sensors:**
- Touch interface: described as “touch sensor/panel” used for gesture-like interaction. citeturn27view0turn28view0turn17view0  
- IMU: described as “six-axis IMU” (accelerometer + gyroscope). citeturn4view0turn17view0turn25search24  
- Microphone: explicitly stated; voice interaction/commands reported. citeturn27view0turn28view0turn29view0  
- Vibration: described in multiple summaries as “vibration and touch” for “emotion sensing.” citeturn19view0turn13view0  

**Interpretation note (Confidence: Medium):** “vibration sensor” vs “vibration motor” is ambiguous in secondary write-ups. The phrasing “senses your emotions through vibration and touch sensors” could refer to:
- a **vibration motor** for haptic output (and the system “senses” your interaction via touch + IMU), or  
- an actual **vibration sensor** (piezo) used to detect taps/knocks.  
No accessible source in this review provides a component-level clarification. citeturn19view0turn13view0

### Wireless, charging, and battery subsystem

**Confirmed:**
- Wi‑Fi + Bluetooth connectivity are repeatedly stated. citeturn27view0turn28view0turn15search12turn17view0  
- Battery specified at **800 mAh**, with typical runtime described as **~4 hours** (some sources: “4–6 hours depending on usage”). citeturn27view0turn28view0turn22view0  
- USB‑C is reported as the charging port. citeturn27view0turn28view0turn29view0  

**Speculative electrical design (Confidence: Medium):**
- Cell chemistry is almost certainly a **single-cell 3.7 V Li‑ion polymer (LiPo)** pack (800 mAh class), because it fits the volume and is typical for compact toys.
- USB‑C likely operates in **USB 2.0 / 5 V sink-only** mode (no PD negotiation required).
- Charge management likely uses a small **single-cell Li‑ion charger IC** with power-path or load-sharing, plus a **3.3 V regulator** for the ESP32‑S3.

## Firmware, software, and networking stack

### TurtleOS, “motion control system,” and the app layer

**Confirmed:**
- The on-device software is branded **TurtleOS**. citeturn19view0turn17view0turn27view0turn28view0  
- The vendor describes a purpose-built motion control system and OS to drive precise/dynamic movements. citeturn13view0turn17view0  
- A drag-and-drop coding interface (often described as Blockly-like blocks) is repeatedly claimed. citeturn13view0turn17view0turn27view0  
- Official FAQ: KT2 can work offline; no subscription; free updates for backers. citeturn22view0  

**Public ambiguity about “apps” (Confidence: Medium):**
- Some coverage suggests the control experience was (at least at one point) “web app now, mobile app later.” citeturn17view0  
- Other summaries state iOS and Android apps exist. citeturn25search24  
Given these conflicts and lack of directly accessible store listings in the sources here, the safest conclusion is: **KT2 uses an external UI (web and/or mobile) for selection of games and programming**, but exact delivery (native vs web) is not fully confirmed in accessible sources. citeturn17view0turn25search24

### Firmware update method and likely implementation

**Confirmed:** OTA updates are explicitly described as occurring over **Bluetooth or Wi‑Fi**. citeturn28view0turn27view0turn29view0

**Speculative implementation (Confidence: Medium-High):**
- If KT2 uses ESP32‑S3 + ESP-IDF or a similar stack, it can adopt Espressif’s OTA framework, which supports updating a device “while normal firmware is running” over connectivity channels including Wi‑Fi and Bluetooth. citeturn36search16  
- A common pattern is HTTPS-based OTA (download new image over TLS) using `esp_https_ota`. citeturn36search0  
- The “over Bluetooth” phrasing could mean either:
  - the firmware payload is transported over BLE to the device, which then writes it to an OTA partition, or
  - BLE is used to provision Wi‑Fi / trigger an update, while the actual download happens via Wi‑Fi.

### Inferred system architecture diagram

The following diagram is **an inferred architecture** that is consistent with the public statements (ESP32‑S3 class device, Wi‑Fi/BLE, OTA updates, touch/IMU/mic sensing, active motion control, external app/web UI). citeturn27view0turn28view0turn4view0turn36search16turn36search0

```mermaid
flowchart LR
  subgraph Client["User-side UI"]
    Phone["Mobile app (native or web-wrapper)"]
    WebUI["Web UI (Blockly-like programming)"]
    Controller["Optional controller (bundle-dependent)"]
  end

  subgraph Robot["KT2 robot (inferred)"]
    ESP["ESP32-S3 class MCU/SoC"]
    IMU["6-axis IMU"]
    Touch["Touch surface/panel"]
    Mic["Microphone"]
    LEDs["Status/interaction LEDs (implied by 'lighting interaction')"]
    Actuators["4 leg actuators (servos)"]
    PMIC["Battery + charger + regulators"]
    OTA["OTA partitions + bootloader"]
    Motion["Motion control / gait + stunt library"]
    Games["On-device game logic + state machines"]
  end

  WebUI -->|programs / macros| Phone
  Phone -->|BLE GATT control| ESP
  Phone -->|Wi-Fi (LAN) control| ESP
  Controller -->|local RF or BLE (unknown)| ESP

  ESP --> Motion --> Actuators
  ESP --> Games --> Motion
  IMU --> ESP
  Touch --> ESP
  Mic --> ESP
  ESP --> LEDs

  ESP --> OTA
  OTA -->|firmware swap| ESP
  Phone -->|trigger update| ESP
  ESP -->|download via HTTPS (if Wi-Fi used)| OTA
```

## Manufacturing, compliance, and IP landscape

### Assembly model and enclosure integration

The DIY messaging (“12 parts,” “20 steps”) implies manufacturing choices geared toward **low assembly complexity for end users**: large snap-fit shells, minimal wiring exposure, and preassembled submodules (actuator pack + main PCB + battery). citeturn22view0turn19view0turn27view0

The presence of a USB‑C port and multiple actuators implies internal harnessing; a Kickstarter comment snippet (not directly accessible in full here) even mentions “servo cables,” consistent with a connectorized internal design. citeturn32search11turn27view0

### Estimated KT2-like BOM and COGS range

Because no official teardown/BOM is available in accessible sources, the table below is a **clearly-labeled speculative estimate**, built from confirmed top-level specs (ESP32‑S3 class, 4 actuators, 800 mAh cell, Wi‑Fi/BLE, IMU, mic, touch, USB‑C). citeturn27view0turn28view0turn4view0turn14view0

Prices are **typical mass-production component costs** (high volume) and are provided as ranges; this is not a claim about the creator’s actual suppliers.

| Subsystem | Likely parts (examples) | Qty | Est. unit cost (USD) | Est. extended | Status | Confidence |
|---|---|---:|---:|---:|---|---|
| Compute + RF | ESP32‑S3 module (Wi‑Fi+BLE) | 1 | 2.5–5.0 | 2.5–5.0 | Inferred from public MCU claim | Medium |
| IMU | 6‑axis accel/gyro (BMI270 / ICM‑426xx class) | 1 | 0.6–1.8 | 0.6–1.8 | Inferred from “6-axis IMU” | Medium |
| Microphone | MEMS mic (I2S/PDM) | 1 | 0.4–1.2 | 0.4–1.2 | Inferred from mic claim | Medium |
| Touch | Capacitive electrodes + MCU touch peripheral (or touch IC) | 1 | 0.0–0.6 | 0.0–0.6 | Partially confirmed (touch exists; IC unclear) | Medium |
| Actuation | Micro servos / feedback servos (“five-wire” upgrade mentioned) | 4 | 3.0–8.0 | 12–32 | Inferred from “4-joint” + “five-wire servos” note | Medium |
| Power cell | 1S LiPo 800 mAh pack + protection | 1 | 1.5–3.5 | 1.5–3.5 | Confirmed capacity; chemistry inferred | Medium-High |
| Charging | USB‑C receptacle + Li‑ion charger IC | 1 | 0.6–1.6 | 0.6–1.6 | USB‑C confirmed; charger IC inferred | Medium |
| Regulation | 3.3 V LDO/buck + power-path parts | 1 | 0.4–1.2 | 0.4–1.2 | Inferred | Medium |
| Main PCB | 4–6 layer PCB + SMT assembly + passives | 1 | 3–7 | 3–7 | Inferred | Medium |
| LEDs | 1–4 small RGB/mono LEDs + driver/resistors | 1 | 0.1–0.6 | 0.1–0.6 | “Lighting interaction” suggests some light output | Low-Medium |
| Enclosure | Injection-molded shells + legs + fasteners | 1 | 2–6 | 2–6 | Inferred | Medium |
| Packaging | Box + inserts | 1 | 0.5–2.0 | 0.5–2.0 | Inferred | Medium |

**Speculative COGS total:** roughly **$24–$61** depending on actuator cost (dominant driver), PCB cost, and enclosure tooling amortization. (Confidence: Low-Medium; intended as an order-of-magnitude estimate.)

### Safety and regulatory considerations

If KT2 (or a similar robot) is commercialized broadly, several compliance regimes are typically relevant:

**Radio + EMC**
- In the U.S., digital devices can fall under FCC Part 15 (including Subpart B for unintentional radiators), and the FCC provides an “Equipment Authorization” framework for RF devices. citeturn37search3turn37search7  
- For the EU, Wi‑Fi/BLE products generally fall under the **Radio Equipment Directive (RED) 2014/53/EU**, which sets essential requirements for safety, EMC, and efficient spectrum use. citeturn36search15turn36search7  

**Battery safety + transport**
- Lithium battery shipments typically require UN 38.3 transport testing; the UN Manual of Tests and Criteria subsection 38.3 defines the procedures. citeturn37search16turn37search0  
- Portable rechargeable lithium batteries are often evaluated against IEC 62133‑2 safety requirements in many markets. citeturn37search21turn37search1  

**Toy safety (if marketed to children)**
- In the U.S., the CPSC provides guidance on ASTM F963 applicability; ASTM F963 is the well-known toy safety specification (sold by ASTM). citeturn37search22turn37search14  

### IP signals

A relevant IP data point discoverable in accessible public sources is a **“WAIRLIVING”** trademark filing (in apparel contexts) listed by entity["organization","Justia","legal information platform"]. citeturn34search1

No reliably accessible sources in this research pass surfaced:
- a specific registered trademark for “KT2” or “Kungfu Turtle” tied to the robot product name, or  
- patents clearly and directly associated with KT2’s motion system or TurtleOS.

This does **not** mean no such filings exist; rather, they were not located in the sources accessible within this review.

## Community and reverse-engineering signals

### Reviews, demonstrations, and community posts

Public community content primarily takes the form of **reviews and demo videos** (often hosted on YouTube, but mirrored/embedded elsewhere), along with reposts on social platforms and Reddit. For example, an independent “review/assembly” video page exists on a creator site (Eric J. Kuhns), implying hands-on availability and assembly experience in the community. citeturn31view0

Reddit communities have posted KT2 clips and links back to the campaign. citeturn33view0

Mainstream maker/gadget press coverage exists (e.g., entity["organization","Hackster.io","maker news site"] and entity["organization","CoolThings.com","gadget site"]), but these generally restate feature claims rather than providing teardowns. citeturn4view0turn17view0

### Teardowns, FCC filings, and repos

Within the accessible sources used here, no high-quality teardown (PCB photos, chip IDs, or firmware extraction notes) and no end-product FCC ID filing package were found.

A campaign tracker highlights “Open Source Case File” in an update headline, but does not provide the referenced assets in an accessible way here. citeturn14view0turn23search0

**Speculation (Confidence: Low):** if “Open Source Case File” refers to mechanical CAD (shell/legs) rather than firmware, it would match the marketing emphasis on customization (stickers, 3D prints) while keeping the motion firmware proprietary. This is a common pattern in consumer robotics.

## Reproduction plan for a tabletop waiter robot inspired by KT2

This plan aims to reproduce the **spirit** of KT2—compact, expressive, modular, and approachable DIY—while changing the mission to a **tabletop “waiter”**: carry a small tray (e.g., snacks, a note, a remote, small tools) across a desk/table, avoid obstacles, and interact via touch/voice prompts. It is **not** a clone, and intentionally avoids copying TurtleOS or any proprietary assets.

### Target requirements

A practical “tabletop waiter” spec (recommended):
- Footprint ≤ 160 × 160 mm; height ≤ 120 mm.
- Payload: 150–300 g on a top tray.
- Runtime: 2–4 hours typical.
- Works offline; optional Wi‑Fi for OTA updates and a browser control UI.
- Simple assembly (3D printed chassis + off-the-shelf electronics).

### Architecture choice

A quadruped like KT2 is mechanically challenging at small scale. For a “waiter,” stability and payload matter more than flips. The simplest robust approach is:

**Recommended base:** 2-wheel differential drive + rear caster (or 4-wheel skid steer)  
**Optional “KT2-inspired expressiveness”:** add a 1–2 DoF “head” or “gesture arm” using micro servos + LEDs for personality.

This retains the **small interactive companion** ethos while making the “carry a tray” goal realistic.

### Bill of materials and estimated build cost

Below is a complete build BOM, with substitutions.

| Subsystem | Part (example class) | Qty | Typical hobbyist cost (USD) | Substitutions |
|---|---|---:|---:|---|
| Compute + wireless | ESP32‑S3 dev board | 1 | 10–20 | Any ESP32 board (S3 preferred for headroom) |
| Motors | 6 V micro metal gear motors (N20) | 2 | 20–40 (pair) | TT motors (cheaper, bulkier, noisier) |
| Encoders | Magnetic/optical encoders for N20 (optional but recommended) | 2 | 10–25 | Run open-loop (reduced accuracy) |
| Motor driver | TB6612FNG (or DRV8833 class) | 1 | 3–10 | Any dual H-bridge with ≥1 A/channel peak |
| Power cell | 1S LiPo 2000–3000 mAh *or* 2×18650 pack | 1 | 12–30 | Larger LiPo if space permits |
| Charging | USB‑C 1S Li‑ion charger board (with protection) | 1 | 5–15 | Dedicated charger IC board; add power-path if needed |
| Regulation | 5 V boost (if 1S) + 3.3 V rail | 1 | 5–12 | Use 2S pack + buck to 5 V |
| Sensing | 2–3× ToF sensors (front + sides) | 2–3 | 10–30 | Ultrasonic sensors (bulkier) |
| IMU | 6‑axis IMU module | 1 | 2–8 | Optional if using encoders only |
| UI | Capacitive touch pad copper tape | 1 | 1–5 | Buttons, or a small joystick |
| Audio | I2S MEMS mic + small buzzer/speaker | 1 each | 5–15 | Skip mic; keep buzzer only |
| Lighting | 4–12 WS2812-compatible RGB LEDs | 1 strip | 2–8 | Single status LED |
| Mechanics | 3D printed chassis + tray + brackets | 1 | 5–20 (material) | Laser-cut acrylic + standoffs |
| Hardware | Screws, heat-set inserts, wiring | 1 | 5–15 | — |

**Estimated total:** **$70–$200** depending on motors (with/without encoders), battery choice, and how polished you make the enclosure.

### Mechanical drawings and build notes

**Suggested envelope (example):**
- Base: 140 mm (L) × 120 mm (W)
- Wheel diameter: 45–60 mm
- Tray: 120 mm × 90 mm with a 10–15 mm lip
- Battery centered low, motors near rear for traction

Simple dimension sketch (conceptual):

```text
TOP VIEW (mm)
+--------------------------------------------------+
|                Tray 120 x 90                     |
|          +--------------------------+            |
|          |                          |            |
|          |                          |            |
|          +--------------------------+            |
|                                                  |
|  [ToF]                                     [ToF] |
|                                                  |
|   O  Left wheel                      Right wheel O
|                 (caster) o                         |
+--------------------------------------------------+
Base: 140 (L) x 120 (W)
```

Key mechanical principles:
- Keep the battery **lowest** to avoid tip-over with payload.
- Use a tray lip and optionally a silicone mat to prevent sliding.
- Provide an easy-access battery compartment and a protected USB‑C cutout.

### Firmware/software stack recommendation

This stack mirrors what is *plausible* for KT2 (ESP32-class + OTA), but uses fully documented, open tooling:

- **Firmware base:** ESP-IDF (uses FreeRTOS-style tasking and standard peripherals).  
- **OTA updates:** ESP-IDF OTA mechanism + optional HTTPS OTA helper (common for Wi‑Fi connected devices). citeturn36search16turn36search0  
- **Optional robotics middleware:** micro-ROS client on the ESP32 + ROS 2 on a laptop/RPi for higher-level autonomy (mapping, task queues). micro-ROS has documented RTOS workflows and has been ported to ESP32 using FreeRTOS. citeturn36search2turn36search10  
- **On-device control model:**  
  - Task 1: motor control loop (100–500 Hz if encoders; otherwise 50–100 Hz)  
  - Task 2: sensor fusion (IMU + ToF + bump/touch)  
  - Task 3: behavior planner (finite-state machine: idle → navigate → dock → deliver)  
  - Task 4: comms (BLE + Wi‑Fi HTTP)  
  - Task 5: UI (LED animations, buzzer, voice trigger if implemented)

### Testing and validation procedures

Functional and safety testing should be treated as engineering deliverables.

**Core functional tests**
- Motor direction + stall current test (verify driver thermal margin).
- Payload test: run with 0 g / 150 g / 300 g payload and measure tip-over margin.
- Navigation test: obstacle detect/stop at multiple approach speeds; validate ToF placement.

**Reliability tests**
- 2-hour endurance run on a loop path; log resets and brownouts.
- Drop/impact test from a small height (e.g., 20–30 cm onto a mat) to check connectors and battery retention.

**OTA/update tests**
- Corrupt image test (intentionally interrupted update) to confirm rollback behavior (two-partition design is typical for OTA frameworks). citeturn36search16turn36search0  

**Compliance-aware prechecks (if you ever productize)**
- U.S. FCC Part 15 awareness for digital devices and RF devices—especially if adding Wi‑Fi/BLE. citeturn37search7turn37search3  
- EU RED awareness if selling in the EU/EEA. citeturn36search15turn36search7  
- Lithium battery transport testing expectations (UN 38.3) if shipping cells/packs. citeturn37search16turn37search0  

### Suggested “KT2-inspired” personality layer

To capture the charm of KT2 without copying:
- Create an “expressiveness module”: LED eyes + a 1-DoF nodding “head” servo.
- Implement “desk companion modes”: Pomodoro reminders, “delivery ready” animations, gentle haptic/audio confirmation (KT2 is marketed similarly as a productivity companion). citeturn22view0turn13view0turn17view0