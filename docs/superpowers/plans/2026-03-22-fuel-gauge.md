# Fuel Gauge Display — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Display real-time fuel level from an ELM327 OBD2 adapter on the ESP32 CYD's ILI9341 screen as a colour-coded horizontal bar gauge.

**Architecture:** A state machine drives the application through BT connecting, OBD2 initialising, running, and error/retry states. Each state has its own display rendered via a full-screen TFT_eSPI sprite (flicker-free). ELMduino's non-blocking API is polled in `loop()` with no `delay()` blocking.

**Tech Stack:** ESP32 Arduino (PlatformIO), TFT_eSPI, BluetoothSerial, ELMduino

---

## Hardware Context

| Component | Detail |
|-----------|--------|
| Board | ESP32-2432S028R "CYD" (ESP32-WROOM-32, ST7789 320x240) |
| OBD2 adapter | ELM327 Bluetooth Classic |
| Power | 5V buck converter from fuse tap to CYD USB port |
| Display orientation | Landscape (320 wide x 240 tall) |

---

## CYD ST7789 Pinout (TFT_eSPI)

The CYD drives its ST7789 via **HSPI**, not the default VSPI. You **must** pass `-DUSE_HSPI_PORT` or the display will not work.

| Define | GPIO | Notes |
|--------|------|-------|
| `TFT_MOSI` | 13 | HSPI MOSI |
| `TFT_MISO` | 12 | HSPI MISO |
| `TFT_SCLK` | 14 | HSPI CLK |
| `TFT_CS` | 15 | HSPI chip select |
| `TFT_DC` | 2 | Data/Command |
| `TFT_RST` | -1 | Not connected on CYD |
| `TFT_BL` | 21 | Backlight, active HIGH |
| `TFT_RGB_ORDER` | TFT_BGR | ST7789 colour order (blue-green-red) |
| `TFT_INVERSION_OFF` | — | Required for correct ST7789 colours |
| SPI freq | 55 MHz | Proven stable on this CYD variant |

**Touch (XPT2046) is on a separate SPI bus — not needed for this project and should be removed from the build to save RAM.**

Other occupied GPIOs to avoid: 4/16/17 (RGB LED), 5/18/19/23 (SD card), 26 (speaker), 34 (LDR), 36/32/39/25/33 (touch).

---

## Project Structure & File Layout

```
CarDashboard/
  platformio.ini              # Board, libs, TFT build flags
  src/
    main.cpp                  # setup(), loop(), wiring
    state_machine.h           # AppState enum, transition logic
    state_machine.cpp
    obd_reader.h              # BluetoothSerial + ELMduino wrapper
    obd_reader.cpp
    gauge_display.h           # Sprite-based gauge rendering
    gauge_display.cpp
    config.h                  # Timing constants, BT name, colours
  docs/
    superpowers/plans/
      2026-03-22-fuel-gauge.md  # This plan
```

### File responsibilities

| File | Responsibility |
|------|---------------|
| `config.h` | All tuneable constants: BT device name, poll interval, timeout, colour thresholds, bar dimensions. Single source of truth for magic numbers. |
| `state_machine.h/cpp` | `AppState` enum (`BT_CONNECTING`, `OBD_INIT`, `RUNNING`, `ERROR`), transition functions, retry counters. No display or OBD code — pure state logic. |
| `obd_reader.h/cpp` | Wraps `BluetoothSerial` and `ELM327`. Exposes `begin()`, `poll()`, `getFuelLevel()`, `isConnected()`, `getErrorCode()`. Handles the non-blocking ELMduino pattern internally. |
| `gauge_display.h/cpp` | Owns the `TFT_eSprite`. Exposes `begin()`, `drawConnecting()`, `drawInitialising()`, `drawGauge(float pct)`, `drawError(const char* msg)`. All rendering composited into sprite then pushed to screen. |
| `main.cpp` | Creates instances, calls `setup()` on each module, runs the state machine switch in `loop()`. |

---

## platformio.ini Configuration

The existing `src/Setup_ESP32_2432S028R_ST7789.h` header already defines all the correct TFT_eSPI settings for this board. We keep using the `-include` approach from the current `platformio.ini` rather than duplicating every flag inline. This keeps the pin definitions in one authoritative file.

```ini
[env:nodemcu-32s]
platform = espressif32
board = nodemcu-32s
framework = arduino
monitor_speed = 115200

lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    powerbroker2/ELMduino@^3.4

build_flags =
    -DUSER_SETUP_LOADED=1
    -include src/Setup_ESP32_2432S028R_ST7789.h
```

**Key points:**
- `Setup_ESP32_2432S028R_ST7789.h` is **kept** — it defines ST7789_DRIVER, HSPI, BGR colour order, inversion off, 55 MHz SPI, backlight, and all pin assignments. No reason to duplicate this into build_flags.
- XPT2046_Touchscreen dependency **removed** — not needed for fuel gauge, saves flash and avoids VSPI bus contention
- `ELMduino@^3.4` **added** — the latest v3 API with non-blocking support
- No WiFi — keeps RAM free for BluetoothSerial (~140 KB)

---

## Sprite-Based Rendering

All drawing composites into a single 320x240 16-bit `TFT_eSprite` before calling `pushSprite(0, 0)`. This eliminates flicker entirely — no partial draws are ever visible.

**Memory cost:** 320 x 240 x 2 bytes = **153,600 bytes** (150 KB). This is tight alongside BluetoothSerial's ~140 KB. Total ~290 KB of ~520 KB SRAM. Workable but leaves little headroom.

**Fallback if RAM is too tight:** Use an 8-bit colour sprite (`createSprite` with `setColorDepth(8)`), halving sprite memory to ~75 KB. Colour fidelity drops from 65K to 256 colours — acceptable for a fuel gauge.

### Rendering functions

| Function | What it draws |
|----------|--------------|
| `drawConnecting()` | Black background, "CONNECTING..." in white, centred, large font |
| `drawInitialising()` | Black background, "INITIALISING OBD..." in white |
| `drawGauge(float pct)` | The main gauge: background, bar outline, filled bar with colour gradient, percentage text |
| `drawError(const char* msg)` | Black background, "NO SIGNAL" or custom message in red, centred |

### Gauge layout (landscape 320x240)

```
+------------------------------------------+
|                                          |
|    +----------------------------------+  |
|    |  ████████████████░░░░░░░░░░░░░░  |  |   <- fill bar (colour-coded)
|    +----------------------------------+  |
|                                          |
|                  67%                     |   <- percentage text
|                                          |
+------------------------------------------+
```

- Bar outer rect: x=20, y=70, w=280, h=60
- Fill rect: x=22, y=72, w=up to 276 (mapped from 0-100%), h=56
- Percentage text: centred at x=160, y=160, Font 7 (7-segment style, large)

### Colour gradient logic

Map fuel percentage to a colour using simple thresholds:

| Fuel % | Colour | Hex |
|--------|--------|-----|
| > 50% | Green | `0x07E0` |
| 25–50% | Yellow | `0xFFE0` |
| < 25% | Red | `0xF800` |

Alternatively, a smooth HSV interpolation from red (hue=0) at 0% to green (hue=120) at 100%, converted to RGB565. The simple threshold version is recommended initially — smooth gradient is a nice-to-have refinement.

---

## State Machine Design

```
                    ┌──────────────┐
         ┌─────────┤ BT_CONNECTING │◄────── (initial state / on BT disconnect)
         │         └──────┬───────┘
         │                │ BT connected
         │                ▼
         │         ┌──────────────┐
         │         │   OBD_INIT   │
         │         └──────┬───────┘
         │                │ myELM327.begin() succeeds
         │                ▼
         │         ┌──────────────┐
  timeout│         │   RUNNING    │◄──── normal operation
         │         └──────┬───────┘
         │                │ ELM_NO_RESPONSE / ELM_TIMEOUT (consecutive)
         │                ▼
         │         ┌──────────────┐
         └────────►│    ERROR     │───── retry timer expires → BT_CONNECTING
                   └──────────────┘
```

### States

| State | Display | Action |
|-------|---------|--------|
| `BT_CONNECTING` | "CONNECTING..." | Call `SerialBT.connect()`. Retry every 5s. |
| `OBD_INIT` | "INITIALISING OBD..." | Call `myELM327.begin(SerialBT, false, 5000)`. On failure, back to `BT_CONNECTING`. |
| `RUNNING` | Gauge with bar + percentage | Poll `myELM327.fuelLevel()` non-blocking. Update display on `ELM_SUCCESS`. |
| `ERROR` | "NO SIGNAL" in red | After N consecutive errors or BT disconnect. Wait 5s then retry from `BT_CONNECTING`. |

### Transition rules

- `BT_CONNECTING → OBD_INIT`: `SerialBT.connect()` returns true
- `OBD_INIT → RUNNING`: `myELM327.begin()` returns true
- `OBD_INIT → BT_CONNECTING`: `begin()` fails (retry connection)
- `RUNNING → ERROR`: 3+ consecutive `ELM_NO_RESPONSE` or `ELM_TIMEOUT`, or `SerialBT.connected()` returns false
- `ERROR → BT_CONNECTING`: After 5-second pause

### Retry counter

A `consecutiveErrors` counter increments on each failed poll and resets on `ELM_SUCCESS`. Threshold of 3 triggers the `ERROR` state. This prevents a single dropped packet from killing the display.

---

## ELMduino Integration

### Initialisation sequence

```
1. SerialBT.begin("FuelGauge", true)     // master mode
2. SerialBT.setPin("1234")               // default ELM327 PIN
3. SerialBT.connect("OBDII")             // or by MAC address
4. myELM327.begin(SerialBT, false, 5000) // 5s timeout for clone compat
```

### Non-blocking poll pattern

```
In RUNNING state, each loop() iteration:
  1. Call myELM327.fuelLevel()
  2. Check myELM327.nb_rx_state:
     - ELM_SUCCESS      → store value, reset error counter, redraw gauge
     - ELM_GETTING_MSG  → do nothing (still waiting)
     - anything else    → increment error counter, optionally log
```

**Critical:** Do NOT call `fuelLevel()` again while `nb_rx_state == ELM_GETTING_MSG`. The function must only be called once per query cycle. Use a boolean flag (`queryInProgress`) or only call when state is not `ELM_GETTING_MSG`.

### Poll interval

Fuel level changes slowly — polling every 2-3 seconds is sufficient. After a successful read, wait before starting the next query. Use `millis()` delta, not `delay()`.

```
if (!queryInProgress && millis() - lastPollTime >= POLL_INTERVAL_MS) {
    myELM327.fuelLevel();
    queryInProgress = true;
}
// ... check nb_rx_state, on completion:
queryInProgress = false;
lastPollTime = millis();
```

---

## Loop Structure

```
loop() {
    switch (appState) {
        case BT_CONNECTING:
            // Draw "CONNECTING..." screen BEFORE calling connect()
            // NOTE: SerialBT.connect() is BLOCKING (~10s timeout)
            // so the display must be updated before the call, not after.
            // Attempt connect, on success → OBD_INIT
            // On failure, wait 5s (millis-based) then retry

        case OBD_INIT:
            // Call myELM327.begin() once
            // This IS blocking (~5s timeout) — acceptable during init
            // On success → RUNNING
            // On failure → BT_CONNECTING

        case RUNNING:
            // If no query in progress and poll interval elapsed:
            //   Start fuelLevel() query
            // If query in progress:
            //   Check nb_rx_state
            //   On ELM_SUCCESS: update display, reset errors
            //   On error: increment error counter
            //   On 3+ errors: → ERROR
            // If BT disconnected: → ERROR

        case ERROR:
            // Display: drawError("NO SIGNAL")
            // After 5s pause → BT_CONNECTING
    }
}
```

**No `delay()` calls in the main loop.** All timing uses `millis()` comparisons. The display only redraws when data changes or state transitions — not every loop iteration.

---

## Error Handling & Reconnect Logic

| Scenario | Detection | Response |
|----------|-----------|----------|
| BT won't connect | `SerialBT.connect()` returns false | Retry every 5s, stay on "CONNECTING..." screen |
| ELM327 init fails | `myELM327.begin()` returns false | Back to BT_CONNECTING (adapter may have powered off) |
| Single bad poll | `nb_rx_state` is error but < 3 consecutive | Log it, keep displaying last good value |
| Sustained OBD failure | 3+ consecutive errors | Transition to ERROR state, show "NO SIGNAL" |
| BT disconnect mid-run | `SerialBT.connected()` returns false | Immediate transition to ERROR → reconnect |
| Vehicle doesn't support PID 0x2F | `ELM_NO_DATA` returned | Show "UNSUPPORTED" on display, stop polling |

### Last-known-good value

When a poll fails but we haven't hit the error threshold, continue displaying the last successful fuel reading. Only blank the gauge on entering the ERROR state.

---

## Known Gotchas & Mitigations

### ELM327 Clones

- **Long first connect:** Cheap clones do a full protocol search. The 5000 ms timeout in `begin()` handles most; increase to 30000 ms if needed.
- **Device name varies:** Some clones advertise as "OBDII", others as "OBD2", "ELM327", etc. Consider making the BT name configurable in `config.h`, or connect by MAC address for reliability.
- **PIN code:** Most use "1234", some use "0000" or "6789". Configurable in `config.h`.
- **Baud rate:** Library auto-detects, but if connection is flaky, try setting protocol explicitly in `begin()`.

### BluetoothSerial.connect() Is Blocking

- `SerialBT.connect()` blocks for up to 10+ seconds while scanning and pairing. There is no async alternative in the Arduino BluetoothSerial library.
- **Mitigation:** Always render the "CONNECTING..." screen *before* calling `connect()`. The display will freeze during the call but the user sees the correct status. Similarly, `myELM327.begin()` blocks for up to its timeout value (5s).
- If the blocking UX is unacceptable, the only workaround is running BT connection on a FreeRTOS task. This is out of scope for v1 but noted for future improvement.

### ELMduino Timing

- **Don't re-send while waiting:** Calling `fuelLevel()` when `nb_rx_state == ELM_GETTING_MSG` re-sends the AT command and corrupts the response. The `queryInProgress` flag prevents this.
- **`ELM_TIMEOUT` false positives:** Sometimes caused by extra bytes in the response, not actual timeouts. Enable debug mode (`begin(stream, true, ...)`) during development to inspect raw traffic on Serial Monitor.
- **Fuel level accuracy:** Some vehicles return erratic values. If readings look wrong, try the manual calculation: `(100.0 / 255.0) * myELM327.responseByte_0`.

### TFT_eSPI on CYD

- **Must use HSPI:** The `USE_HSPI_PORT` define is non-negotiable. Without it, the display won't initialise.
- **`TFT_RST` must be -1:** The CYD has no reset pin wired to the display. Setting it to any GPIO will cause init failure.
- **Backlight on GPIO 21:** Set `TFT_BACKLIGHT_ON=HIGH`. Some board revisions have it hardwired always-on.
- **ST7789 requires BGR + inversion off:** Without `TFT_RGB_ORDER TFT_BGR` and `TFT_INVERSION_OFF`, colours will be swapped or inverted. Both are defined in the existing setup header.
- **ST7789 vs ILI9341 variant:** Some CYDs ship with ILI9341 instead. If the display is blank or garbled, try changing `ST7789_DRIVER` to `ILI9341_DRIVER` in the setup header and removing `TFT_INVERSION_OFF`.

### Memory Pressure

- BluetoothSerial: ~140 KB RAM
- 16-bit sprite (320x240): ~150 KB RAM
- Total: ~290 KB of ~520 KB available
- **If heap runs low:** Switch sprite to 8-bit colour depth (`setColorDepth(8)`) → ~75 KB, bringing total to ~215 KB. Fuel gauge colours (green/yellow/red on black) look fine in 8-bit.
- **Do not enable WiFi** alongside BT Classic — they share the radio and WiFi adds another ~60 KB RAM.

### Power

- The 5V buck converter must supply at least 500 mA. The ESP32 + display draw ~250-350 mA typical, with BT radio peaks higher.
- Poor USB cables cause brownouts and random resets. Use a short, thick cable from the buck converter.

---

## Task Breakdown

### Task 1: Project scaffolding and platformio.ini

**Files:**
- Modify: `platformio.ini`
- Create: `src/config.h`
- Keep: `src/Setup_ESP32_2432S028R_ST7789.h` (existing TFT pin config, unchanged)
- Modify: `src/main.cpp` (strip to empty skeleton)

- [ ] **Step 1:** Update `platformio.ini` — add `powerbroker2/ELMduino@^3.4` to `lib_deps`, remove `XPT2046_Touchscreen` dependency, keep the `-include src/Setup_ESP32_2432S028R_ST7789.h` build flag
- [ ] **Step 2:** Create `src/config.h` with all constants: BT device name ("OBDII"), BT PIN ("1234"), poll interval (3000 ms), error threshold (3), bar dimensions, colour values
- [ ] **Step 3:** Strip `src/main.cpp` to a minimal skeleton: includes, empty `setup()`, empty `loop()`
- [ ] **Step 4:** Build to verify TFT_eSPI compiles with the ST7789 setup header — `pio run`
- [ ] **Step 5:** Commit: "chore: reconfigure project for ST7789 fuel gauge"

---

### Task 2: State machine module

**Files:**
- Create: `src/state_machine.h`
- Create: `src/state_machine.cpp`

- [ ] **Step 1:** Write `state_machine.h` — define `AppState` enum (`BT_CONNECTING`, `OBD_INIT`, `RUNNING`, `ERROR`), declare `transitionTo(AppState)`, `getState()`, `getStateName()`
- [ ] **Step 2:** Write `state_machine.cpp` — implement state storage, transition logging via `Serial.println`, `getStateName()` returns human-readable strings
- [ ] **Step 3:** Build to verify compilation — `pio run`
- [ ] **Step 4:** Commit: "feat: add state machine module"

---

### Task 3: Gauge display module (sprite rendering)

**Files:**
- Create: `src/gauge_display.h`
- Create: `src/gauge_display.cpp`

- [ ] **Step 1:** Write `gauge_display.h` — declare class `GaugeDisplay` with `begin(TFT_eSPI&)`, `drawConnecting()`, `drawInitialising()`, `drawGauge(float pct)`, `drawError(const char* msg)`
- [ ] **Step 2:** Write `gauge_display.cpp`:
  - `begin()`: create sprite (320x240, try 16-bit, fall back to 8-bit if `createSprite` returns nullptr), set text datum
  - `drawConnecting()`: fill black, draw "CONNECTING..." centred in white Font 4, push sprite
  - `drawInitialising()`: fill black, draw "INITIALISING OBD..." centred, push sprite
  - `drawGauge(float pct)`: fill black, draw outer rect, calculate fill width from pct, pick colour from thresholds, draw filled rect, draw pct text in Font 7 below bar, push sprite
  - `drawError(const char* msg)`: fill black, draw message in red Font 4 centred, push sprite
  - Helper `fuelColour(float pct)`: returns green/yellow/red based on thresholds from config.h
- [ ] **Step 3:** Build — `pio run`
- [ ] **Step 4:** Commit: "feat: add sprite-based gauge display module"

---

### Task 4: OBD reader module (BluetoothSerial + ELMduino)

**Files:**
- Create: `src/obd_reader.h`
- Create: `src/obd_reader.cpp`

- [ ] **Step 1:** Write `obd_reader.h` — declare class `OBDReader` with `beginBluetooth()`, `connectBT(const char* name)`, `initELM()`, `poll()`, `getFuelLevel()`, `isConnected()`, `getError()`, `isQueryInProgress()`
- [ ] **Step 2:** Write `obd_reader.cpp`:
  - `beginBluetooth()`: call `SerialBT.begin("FuelGauge", true)` and `setPin()`
  - `connectBT()`: call `SerialBT.connect(name)`, return success/fail
  - `initELM()`: call `myELM327.begin(SerialBT, false, 5000)`, return success/fail
  - `poll()`: implement the non-blocking pattern — only call `fuelLevel()` if not already in progress and interval has elapsed; check `nb_rx_state`; return a result enum (`POLL_SUCCESS`, `POLL_WAITING`, `POLL_ERROR`)
  - `getFuelLevel()`: return last successful reading
  - `isConnected()`: return `SerialBT.connected()`
  - `getError()`: return `myELM327.nb_rx_state`
- [ ] **Step 3:** Build — `pio run`
- [ ] **Step 4:** Commit: "feat: add OBD reader module with non-blocking polling"

---

### Task 5: Wire everything together in main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1:** Write `main.cpp`:
  - Instantiate `TFT_eSPI`, `GaugeDisplay`, `OBDReader`, state machine
  - `setup()`: init Serial, init TFT (including `setRotation(1)` for landscape, backlight HIGH), init display, init BT, set initial state to `BT_CONNECTING`
  - `loop()`: switch on `appState`:
    - `BT_CONNECTING`: draw connecting screen, attempt `connectBT()` every 5s via millis, on success → `OBD_INIT`
    - `OBD_INIT`: draw initialising screen, call `initELM()`, on success → `RUNNING`, on fail → `BT_CONNECTING`
    - `RUNNING`: call `poll()`, on `POLL_SUCCESS` → `drawGauge(getFuelLevel())`, track consecutive errors, on threshold → `ERROR`, on BT disconnect → `ERROR`
    - `ERROR`: draw "NO SIGNAL", after 5s → `BT_CONNECTING`
- [ ] **Step 2:** Build — `pio run`
- [ ] **Step 3:** Commit: "feat: wire state machine, display, and OBD into main loop"

---

### Task 6: Upload and test on hardware

- [ ] **Step 1:** Connect CYD via USB, upload — `pio run -t upload`
- [ ] **Step 2:** Open serial monitor — `pio device monitor`
- [ ] **Step 3:** Verify: display shows "CONNECTING..." on boot
- [ ] **Step 4:** Power on ELM327 adapter in car, verify BT connects and gauge displays fuel level
- [ ] **Step 5:** Turn off ELM327, verify "NO SIGNAL" appears after ~10s and reconnect cycle begins
- [ ] **Step 6:** If 16-bit sprite causes crashes (OOM), switch to 8-bit in `gauge_display.cpp` and re-test
- [ ] **Step 7:** Final commit: "test: verified on hardware, tuned timeouts"
