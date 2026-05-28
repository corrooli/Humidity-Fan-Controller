# ESP32 Smart Bathroom Fan Controller

An automatic bathroom fan controller built on an ESP32. It monitors humidity and temperature via a Sensirion SHT45 sensor and switches any 12V DC fan through a relay, preventing mold by keeping the bathroom dry after showers.

---

## Features

- **Automatic humidity control** — fan turns on at or above 65% RH, turns off at or below 55% RH
- **Shower spike detection** — detects a rapid humidity rise (5% RH within 60 seconds) and starts the fan immediately, before the upper threshold is even reached
- **Manual override button** — press once to run the fan for 15 minutes regardless of humidity
- **4-hour safety cutoff** — if the fan runs continuously for 4 hours, it is forced off and a mandatory 15-minute cooldown is applied before automatic control resumes
- **Sensor fault detection** — after 5 consecutive failed reads the humidity logic pauses safely and the fan stays off; button override still works; system recovers automatically when the sensor comes back
- **Verbose serial output** — full status line every 10 seconds, all state transitions logged at 115200 baud
- **Safe boot** — relay is driven to OFF as the very first instruction, before any other code runs, so the fan never twitches on at startup

---

## Hardware

### Parts List

| Part | Notes |
|------|-------|
| ESP32 dev board | Any standard 30 or 38-pin ESP32 |
| Sensirion SHT45 | On a breakout board with VCC / GND / SDA / SCL pins. SHT40 or SHT41 also work with the same library |
| 5V single-channel relay module | Active-LOW type (the common blue board) |
| IRFZ44N MOSFET | TO-220 package. Used as a 3.3V→5V level translator for the relay IN signal |
| 1kΩ resistor | Gate series resistor |
| 2.2kΩ–4.7kΩ resistor | Gate-to-GND pulldown. **Do not use 10kΩ in a bathroom** — see Hardware Notes |
| 12V cage PSU | Rated for at least your fan's current draw plus ~1A overhead |
| Adjustable DC-DC buck converter | Dial to exactly 5.0V output before connecting anything |
| Any 12V DC fan | Standard 2-wire 12V fan (red +, black −). This project is not tied to any specific fan model — any 12V DC fan works |
| WAGO connectors (×2) | 3–5 port lever connectors for 12V+ and 12V− distribution |
| Momentary push button | Normally open, any type |
| Enclosure | Sealed/weatherproof strongly recommended for bathroom mounting |

---

## Wiring

> ⚠️ **MAINS WARNING:** This project connects to 230V (or 120V) AC mains on the PSU input side. The brown/blue/green-yellow wires going into the PSU cage must be wired correctly. If you are not comfortable and competent with mains wiring, have a qualified electrician wire the AC input side. All DC wiring downstream of the PSU is safe to do yourself.

---

### AC Mains Side

| From | Wire | To | Terminal |
|------|------|----|----------|
| Wall cable | Brown — Live | 12V PSU | L |
| Wall cable | Blue — Neutral | 12V PSU | N |
| Wall cable | Green/Yellow — Earth | 12V PSU | ⏚ |

---

### 12V DC Distribution

| From | Pin | To | Pin |
|------|-----|----|-----|
| 12V PSU | V+ | WAGO 1 | any slot |
| 12V PSU | V− | WAGO 2 | any slot |
| WAGO 1 | filled slot | Buck converter | IN+ |
| WAGO 1 | filled slot | Relay board | COM (center screw terminal) |
| WAGO 2 | filled slot | Buck converter | IN− |
| WAGO 2 | filled slot | Fan | Black wire (−) |

WAGO 1 is your 12V positive rail. WAGO 2 is your 12V negative rail. Everything that needs 12V or 12V ground connects here.

---

### Buck Converter Output

> Set the buck converter output to exactly **5.0V** with a multimeter before connecting anything to OUT+ / OUT−.

| From | Pin | To | Pin |
|------|-----|----|-----|
| Buck | OUT+ | ESP32 | 5V pin |
| Buck | OUT+ | Relay board | VCC |
| Buck | OUT− | ESP32 | GND (right side) |
| Buck | OUT− | Relay board | GND |

---

### ESP32 → MOSFET → Relay

The ESP32 outputs 3.3V logic. The relay board expects 5V logic. The IRFZ44N bridges this gap: a 3.3V signal from the ESP32 turns the MOSFET on, which pulls the relay IN pin down to ground, which the relay reads as a clean active-LOW trigger.

**IRFZ44N pin order:** hold the component with the **printed label facing you** and legs pointing down. Left to right: **Gate — Drain — Source**.

| From | Pin | To | Pin | Note |
|------|-----|----|-----|------|
| ESP32 | Pin 18 | MOSFET | Gate (left leg) | Via 1kΩ resistor in series |
| MOSFET | Gate (left leg) | MOSFET | Source (right leg) | Via 2.2kΩ–4.7kΩ pulldown resistor |
| MOSFET | Source (right leg) | Buck OUT− | GND | Direct |
| MOSFET | Drain (middle leg) | Relay board | IN | Direct |

The Gate leg has two wires on it: the 1kΩ coming from Pin 18, and the pulldown resistor going to GND/Source. This is correct — the pulldown holds the gate at 0V when the ESP32 is not driving it, which keeps the relay off during boot and power-up.

---

### Relay Board → Fan

| From | Pin | To | Pin |
|------|-----|----|-----|
| Relay board | NO (screw terminal) | Fan | Red wire (+) |

COM on the relay is already connected to 12V via WAGO 1. When the relay energises, NO closes, 12V reaches the fan's red wire, and the fan runs. When the relay is off, NO is open and the fan has no power.

---

### SHT45 Sensor

| From | Pin | To | Pin |
|------|-----|----|-----|
| ESP32 | 3V3 | SHT45 | VCC |
| ESP32 | GND (left side) | SHT45 | GND |
| ESP32 | Pin 21 | SHT45 | SDA |
| ESP32 | Pin 22 | SHT45 | SCL |

The sensor runs on 3.3V from the ESP32's own 3V3 pin, not from the buck 5V rail.

---

### Button

| From | Pin | To | Pin |
|------|-----|----|-----|
| Button | Leg 1 | ESP32 | Pin 14 |
| Button | Leg 2 | ESP32 | GND (right side) |

No external pull-up resistor is needed. The code uses `INPUT_PULLUP` on Pin 14, so the button simply shorts the pin to ground when pressed.

---

## Hardware Notes

### Gate Pulldown Value — Important for Bathrooms

The pulldown resistor from MOSFET Gate to GND is critical for reliable operation in a humid environment. The IRFZ44N gate is a high-impedance node: even tiny leakage currents from humidity, condensation, or flux residue can charge it up toward the turn-on threshold (2–4V) and engage the relay when the ESP32 is commanding OFF.

- **Use 2.2kΩ–4.7kΩ, not 10kΩ.** A stiffer pulldown requires far more leakage current to overcome, making the circuit immune to the humidity-induced ghost-switching that a 10kΩ would allow.
- If you ever see the relay chattering or staying on in humid conditions while serial correctly shows FAN OFF, this resistor is the first thing to check and reduce.

### Clean Your Flux

After soldering, clean all flux residue off the protoboard with **99% isopropyl alcohol**, paying particular attention to the MOSFET legs, the 1kΩ and pulldown resistors, and the relay IN node. Flux is hygroscopic — it absorbs moisture and becomes conductive, which feeds directly into the gate-leakage problem described above.

Let the board dry completely before powering on.

### Conformal Coat the Board

After cleaning and testing, apply a **brush-on acrylic conformal coat** over the entire protoboard except the screw terminals and any connectors. This seals the board against condensation and long-term moisture ingress. Available at any electronics supplier for a few CHF/EUR/USD. One coat is enough; two is better.

Do not coat the SHT45 sensor — it is designed to be exposed to ambient air and humidity.

### Mount Electronics Outside the Wet Zone

The SHT45 sensor is rated for continuous humidity exposure. Your relay board, MOSFET, and protoboard are not. The best long-term installation runs the SHT45 on its I2C wires into the bathroom while the rest of the electronics live in a sealed enclosure outside the wet zone — in a wall cavity, above the ceiling, or in an adjacent room.

---

## Software

### Dependencies

Install these via Arduino IDE Library Manager (Sketch → Include Library → Manage Libraries):

| Library | Author | Notes |
|---------|--------|-------|
| Adafruit SHT4x | Adafruit | Search "Adafruit SHT4x" |
| Adafruit Unified Sensor | Adafruit | Required by the SHT4x library |

### Arduino IDE Board Setup

1. Add ESP32 board support if you haven't already: File → Preferences → Additional Boards Manager URLs → add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. Tools → Board → ESP32 Arduino → **ESP32 Dev Module**
3. Tools → Upload Speed → **115200**
4. Tools → Port → select your ESP32's COM port

### Flashing

1. Connect the ESP32 via USB
2. Open the sketch in Arduino IDE
3. Click Upload
4. Open Serial Monitor at **115200 baud** to watch the boot sequence

---

## Configuration

All tunable parameters are at the top of the sketch:

```cpp
const float HUMIDITY_MAX   = 65.0;   // fan ON at/above this %RH
const float HUMIDITY_MIN   = 55.0;   // fan OFF at/below this %RH
const float HUMIDITY_SPIKE = 5.0;    // %RH jump in 60 s = shower detected
```

```cpp
const unsigned long OVERRIDE_TIME  = 15UL * 60UL * 1000UL;       // manual button run time
const unsigned long MAX_RUN_TIME   = 4UL * 60UL * 60UL * 1000UL; // hard 4-hour cutoff
const unsigned long COOLDOWN_TIME  = 15UL * 60UL * 1000UL;       // post-cutoff rest
const unsigned long READ_INTERVAL  = 2000UL;                     // sensor poll interval
const unsigned long SPIKE_INTERVAL = 60000UL;                    // spike comparison window
```

### Relay Polarity

```cpp
#define RELAY_ACTIVE_HIGH true
```

This sketch uses an IRFZ44N MOSFET driver between the ESP32 and the relay. The MOSFET inverts the signal, so the relay IN pin is pulled LOW to turn on — which is active-LOW behaviour — but the ESP32 drives Pin 18 HIGH to turn the fan on. `RELAY_ACTIVE_HIGH true` reflects the ESP32's perspective after the MOSFET inversion. If you wire the relay IN directly to the ESP32 without a MOSFET driver, change this to `false`.

---

## Logic Flow

```
Boot
 └─ Pin 18 set OUTPUT + RELAY_OFF immediately (before Serial.begin)
 └─ SHT45 detected → baseline reading taken
 └─ Main loop starts

Every 2 s:   Read temperature + humidity from SHT45
Every 60 s:  Compare current humidity to 60-seconds-ago baseline
             └─ Jump ≥ 5% RH → shower spike detected → fan ON

Button press:  Fan ON for 15 minutes (manual override)
               └─ Button ignored if cooldown is active

Fan ON conditions (any one is sufficient):
  - Humidity ≥ 65% RH
  - Shower spike detected
  - Manual override active

Fan OFF conditions (all must be true simultaneously):
  - No manual override active (or override timer expired)
  - Humidity ≤ 55% RH
  - No cooldown active

Safety cutoff:
  - Fan running continuously for 4 hours → forced OFF
  - 15-minute cooldown → automatic control resumes

Sensor fault:
  - 5 consecutive failed reads → humidity logic suspended
  - Fan stays OFF (safe default)
  - Button override still works
  - System recovers automatically on next successful read
```

---

## Serial Output

Connect at **115200 baud**. Example output:

```
##################################################
>>> ESP32 ALIVE - BOOT SEQUENCE STARTING <
##################################################

[SYSTEM] Relay on pin 18 configured (active-HIGH). Fan forced OFF.
[SYSTEM] Button on pin 14 configured (INPUT_PULLUP).
[SYSTEM] Starting I2C  SDA:21  SCL:22
[SYSTEM] Searching for SHT4x sensor...
[SYSTEM] SUCCESS - SHT4x found and responding.
[SYSTEM] Baseline -> Temp: 22.3 C | Humidity: 48.7 %

>>> BOOT COMPLETE - ENTERING MAIN LOOP <

[DATA] Up:10s | Temp:22.3C | Hum:48.7% | Baseline:48.7% | Fan:OFF
[DATA] Up:20s | Temp:22.4C | Hum:49.1% | Baseline:48.7% | Fan:OFF

>>> TRIGGER: SHOWER DETECTED - humidity jumped 6.2% in 60 s <

[HARDWARE] =====> RELAY ON  (reason: shower spike) <=====

[DATA] Up:90s | Temp:24.1C | Hum:71.3% | Baseline:63.1% | Fan:ON (shower spike, run:30s) | [SPIKE]
```

---

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Relay on immediately at boot | Relay module wired directly to ESP32 without MOSFET driver; 3.3V signal not high enough for 5V relay to read as OFF | Add IRFZ44N driver circuit as described in wiring section |
| Relay on at boot, works fine on USB but not on PSU | Gate pulldown too weak; moisture leakage onto gate | Reduce gate pulldown to 2.2kΩ–4.7kΩ; clean flux; conformal coat |
| Relay chatters in humid conditions, serial shows FAN OFF | Humidity-induced gate leakage crossing turn-on threshold | Reduce gate pulldown resistor; clean flux; conformal coat |
| SHT45 not detected | Wiring fault | Check 3V3, GND, and that SDA/SCL are not swapped (SDA=21, SCL=22) |
| Serial shows nothing | Wrong baud rate or wrong COM port | Set Serial Monitor to 115200 baud |
| Fan never turns off | Humidity genuinely above 55% RH, or HUMIDITY_MIN set too high | Let it dry; check thresholds; verify sensor placement is not directly in shower spray |
