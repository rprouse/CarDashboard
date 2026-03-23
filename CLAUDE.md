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

ESP32 CYD (Cheap Yellow Display, ESP32-2432S028R) fuel gauge that reads fuel level from an ELM327 Bluetooth Classic OBD2 adapter and displays it as a colour-coded bar.

**State machine** drives the app through four states: `BT_CONNECTING → OBD_INIT → RUNNING → ERROR`, with each state reflected on the display. Transitions and the main loop live in `main.cpp`.

**Key modules:**

- `config.h` — All tuneable constants (BT name/PIN, poll interval, bar geometry, colours). Application config only; TFT_eSPI hardware config lives in `platformio.ini` build_flags.
- `state_machine.h/cpp` — `AppState` enum and free functions (`setState`, `getState`, `getStateName`). Pure state logic, no I/O.
- `gauge_display.h/cpp` — `GaugeDisplay` class. All rendering goes through a `TFT_eSprite` (full-screen sprite pushed in one shot to avoid flicker). Falls back to 8-bit colour depth if 16-bit sprite allocation fails (OOM).
- `obd_reader.h/cpp` — `OBDReader` class wrapping `BluetoothSerial` + `ELMduino`. The `poll()` method is non-blocking (returns `PollResult::WAITING` while ELMduino is still receiving). `connectBT()` and `initELM()` are blocking.

## Critical Constraints

- **TFT_eSPI pin config is in `platformio.ini` build_flags**, not a User_Setup.h file. The `-DUSER_SETUP_LOADED=1` flag tells TFT_eSPI to skip its own setup headers. Do not create a separate User_Setup.h.
- **HSPI is mandatory** (`-DUSE_HSPI_PORT=1`). The CYD wires its display to HSPI pins (13/12/14/15), not the default VSPI.
- **ST7789 requires BGR colour order and inversion off.** If colours look wrong, check `TFT_RGB_ORDER` and `TFT_INVERSION_OFF` in build_flags.
- **ELMduino is non-blocking** — calling `fuelLevel()` every loop iteration is correct; the library internally gates re-sends via `nb_rx_state`. Do NOT add a guard that prevents calling `fuelLevel()` while `ELM_GETTING_MSG`.
- **Memory is tight** — BluetoothSerial uses ~140 KB RAM, 16-bit sprite uses ~150 KB. Total ~290 KB of 320 KB. Do not enable WiFi alongside BT Classic. If OOM, the display module falls back to 8-bit sprites automatically.
- **Font 7 (7-segment) only supports digits and `: - .`** — render any other characters (like `%`) with a different font.
- **`SerialBT.connect()` blocks for up to ~10 seconds.** Always render the status screen before calling it.
