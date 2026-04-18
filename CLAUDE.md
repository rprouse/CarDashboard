# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
pio run                    # Build
pio run -t upload          # Flash to ESP32 via USB
pio device monitor         # Serial monitor (115200 baud)
pio run -t upload && pio device monitor  # Flash and monitor
```

No unit tests — this is embedded firmware tested on hardware.

## Architecture

ESP32 CYD (Cheap Yellow Display, ESP32-2432S028R) car dashboard that reads OBD2 PIDs from an ELM327 Bluetooth Classic adapter and renders a CRT-styled monochrome green UI: fuel bar (top), speed in 7-segment font (centre), and a bottom row of voltage / fuel rate / coolant. Danger flashes on low fuel, out-of-range voltage, and overheat coolant.

**State machine** drives the app through four states: `BT_CONNECTING → OBD_INIT → RUNNING → ERROR`, with each state reflected on the display. Each state has a dedicated `handle*()` function in `main.cpp`; `loop()` dispatches via a switch. To add a new state: add it to the `AppState` enum, create a `handleNewState()` function, and add it to the switch.

**Key modules:**

- `config.h` — All tuneable constants (BT name/PIN, poll interval, bar geometry, colours). Application config only; TFT_eSPI hardware config lives in `platformio.ini` build_flags.
- `state_machine.h/cpp` — `AppState` enum and free functions (`setState`, `getState`, `getStateName`). Pure state logic, no I/O.
- `gauge_display.h/cpp` — `GaugeDisplay` class. All rendering goes through a `TFT_eSprite` (full-screen sprite pushed in one shot to avoid flicker). `beginFrame()` clears the sprite; draw methods (`drawGauge`, `drawSpeed`, `drawVoltage`, `drawCoolant`, `drawFuelRate`) each return the y offset for the next widget; `endFrame()` runs `applyCrtEffect()` (scanlines) and pushes the sprite. Falls back to 8-bit colour depth if 16-bit sprite allocation fails (OOM).
- `obd_reader.h/cpp` — `OBDReader` class wrapping `BluetoothSerial` + `ELMduino`. The `poll()` method is non-blocking (returns `PollResult::WAITING` while ELMduino is still receiving). `connectBT()` and `initELM()` are blocking.

**OBD polling scheduler** (`obd_reader.cpp:nextPid()`): two-tier cadence. Fast tier alternates `SPEED ↔ FUEL_RATE`; slow tier rotates `FUEL → COOLANT → VOLTAGE`, inserted every `CFG_SLOW_PID_EVERY_N` cycles. First 3 cycles preload each slow PID so initial values aren't `-1`. To add a PID: decide its tier (fast if it changes with throttle, slow if it drifts), extend `ActivePid`, add a case in `dispatchPid()`, and — for fast PIDs — extend the `fastPids[]` array (which will reduce each fast PID's effective rate).

## Critical Constraints

- **TFT_eSPI pin config is in `platformio.ini` build_flags**, not a User_Setup.h file. The `-DUSER_SETUP_LOADED=1` flag tells TFT_eSPI to skip its own setup headers. Do not create a separate User_Setup.h.
- **HSPI is mandatory** (`-DUSE_HSPI_PORT=1`). The CYD wires its display to HSPI pins (13/12/14/15), not the default VSPI.
- **ST7789 requires BGR colour order and inversion off.** If colours look wrong, check `TFT_RGB_ORDER` and `TFT_INVERSION_OFF` in build_flags.
- **ELMduino is non-blocking** — calling `fuelLevel()` every loop iteration is correct; the library internally gates re-sends via `nb_rx_state`. Do NOT add a guard that prevents calling `fuelLevel()` while `ELM_GETTING_MSG`.
- **Memory is tight** — BluetoothSerial uses ~140 KB RAM, 16-bit sprite uses ~150 KB. Total ~290 KB of 320 KB. Do not enable WiFi alongside BT Classic. If OOM, the display module falls back to 8-bit sprites automatically.
- **Font 7 (7-segment) only supports digits and `: - .`** — not currently used, but if re-introduced, render other characters with a different font.
- **`SerialBT.connect()` blocks for up to ~10 seconds.** Always render the status screen before calling it.
- **Rendering pipeline.** `handleRunning()` and self-contained screens follow: `fillSprite` (via `beginFrame()` or directly) → content → `applyCrtEffect()` → `pushSprite()`. If you add a new screen state, follow this exact order.
- **RUNNING and ERROR states redraw every 50ms** (`CFG_ANIM_INTERVAL_MS`) for pulsing glow and scanline animation. Do not add a `screenDrawn` guard that prevents redraws — animations require continuous rendering.
- **Colours are RGB565** (16-bit: 5 red, 6 green, 5 blue). All colour constants in `config.h` use this format. To convert hex RGB to RGB565: `((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)`.
- **Sentinel `-1.0f` means "no data yet"** for `getFuelLevel()` and `getFuelRate()`. Display layer checks `< 0` and renders dashes (`-- L/h` etc.). Don't remap to `0.0f` — that reads as an in-range value and will hide startup/unsupported-PID states.
- **Bottom-row widgets share a y coordinate** (`main.cpp:handleRunning`). `drawVoltage` uses `TL_DATUM` at `CFG_BAR_X`, `drawFuelRate` uses `TC_DATUM` at `CFG_SCREEN_W/2`, `drawCoolant` uses `TR_DATUM` at `CFG_SCREEN_W - CFG_BAR_X`. When adding to this row, use a distinct datum and measure text width to avoid collisions.
