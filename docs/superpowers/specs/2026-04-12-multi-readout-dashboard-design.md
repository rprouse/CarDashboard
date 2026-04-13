# Multi-Readout Dashboard Design

**Date:** 2026-04-12
**Status:** Approved

## Overview

Expand the fuel-only gauge display into a full dashboard showing four OBD2 readouts: fuel level (bar gauge), vehicle speed (large central display), battery voltage, and coolant temperature. All new readouts are text-based, rendered in the existing CRT phosphor green aesthetic.

## Screen Layout

The 320x240 display divides into three visual zones, drawn top-to-bottom:

```
┌──────────────────────────────────────┐
│  [══════════════════════════]         │
│  SYS://FUEL: 72%                     │  Top: fuel bar + label (~60px)
├──────────────────────────────────────┤
│                                      │
│            008 km/h                  │  Middle: speed (~120px)
│                                      │
├──────────────────────────────────────┤
│  VOLT://12.4V            TEMP://89°C │  Bottom: voltage + coolant (~40px)
└──────────────────────────────────────┘
```

### Top Zone — Fuel Gauge

Unchanged from current implementation. Horizontal bar with border, fill, hash marks at 25/50/75%, and "SYS://FUEL: XX%" label below. Low-fuel danger flash when below 20%.

### Middle Zone — Vehicle Speed

- Three digits with leading zeros (e.g., `008`, `085`, `120`)
- Large font to fill most of the available vertical space
- "km/h" label to the right of the speed digits, sharing the same text baseline
- "km/h" rendered in Font 4 (same size as fuel percentage and other labels)

### Bottom Zone — Voltage and Coolant Temperature

- Left-aligned: `VOLT://XX.XV` — battery/alternator voltage from PID 0x42
- Right-aligned: `TEMP://XX°C` — coolant temperature from PID 0x05
- Both rendered in Font 4, same size as fuel percentage text
- Positioned at the same y coordinate, sharing the bottom strip

## Rendering Pipeline

### Frame Composition

Currently `drawGauge()` owns the full pipeline (fill → content → CRT → push). With four draw methods composing a single frame, the pipeline splits into frame control + pure content methods.

New methods on `GaugeDisplay`:

- `beginFrame()` — calls `fillSprite(TFT_BLACK)`
- `endFrame()` — calls `applyCrtEffect()` then `pushSprite(0, 0)`

### Draw Method Signatures

Each content draw method takes a y position and returns the next y position for chaining:

```cpp
void beginFrame();
int32_t drawGauge(float percent, int32_t y);      // fuel bar + label
int32_t drawSpeed(int kph, int32_t y);             // large 3-digit speed + "km/h"
int32_t drawVoltage(float volts, int32_t y);       // bottom-left "VOLT://XX.XV"
int32_t drawCoolant(int tempC, int32_t y);         // bottom-right "TEMP://XX°C"
void endFrame();
```

Note: `drawVoltage` and `drawCoolant` share the same input y — they draw side-by-side, not stacked. Both still return a y for consistency, but only one is used for further chaining (if needed).

### Call Sequence in handleRunning()

```cpp
display.beginFrame();
int32_t y = display.drawGauge(fuel, 0);
y = display.drawSpeed(kph, y);
display.drawVoltage(volts, y);
display.drawCoolant(tempC, y);
display.endFrame();
```

### Status Screens Unchanged

`drawConnecting()`, `drawInitialising()`, and `drawError()` retain their self-contained pipelines (fill → content → CRT → push) since they render alone without composing multiple elements.

## OBD Polling Design

### Multi-PID Scheduler

The existing single-PID `poll()` method is extended with an internal PID scheduler that multiplexes four PIDs over the one ELM327 serial connection.

### PID Table

| PID  | Name           | ELMduino Method     | Unit  | Range       |
|------|----------------|---------------------|-------|-------------|
| 0x0D | Vehicle Speed  | `vehicleSpeed()`    | km/h  | 0-255       |
| 0x2F | Fuel Level     | `fuelLevel()`       | %     | 0-100       |
| 0x05 | Coolant Temp   | `engineCoolantTemp()` | °C  | -40 to 215  |
| 0x42 | Control Module Voltage | `ctrlModVoltage()` | V | 0-65.535  |

### Poll Schedule

- **Poll interval:** 2 seconds (`CFG_POLL_INTERVAL_MS = 2000`)
- **Speed** (primary PID) is polled every cycle
- **Fuel, coolant, voltage** (slow PIDs) rotate into every 5th cycle (`CFG_SLOW_PID_EVERY_N = 5`)

Cycle pattern:

```
Speed → Speed → Speed → Speed → Fuel →
Speed → Speed → Speed → Speed → Coolant →
Speed → Speed → Speed → Speed → Voltage →
(repeat)
```

Each slow PID updates approximately every 30 seconds (every 15th cycle = 5 cycles between slow slots x 3 slow PIDs).

### OBDReader Changes

**New private state:**

- `ActivePid` enum: `SPEED`, `FUEL`, `COOLANT`, `VOLTAGE`
- Stored last-known values: `_speed` (int), `_coolantTemp` (int), `_voltage` (float) alongside existing `_lastFuelLevel` (float)
- `_cycleCount` — incremented after each successful poll or error, drives slow PID insertion
- `_slowPidIndex` — tracks which slow PID is next in rotation (0=fuel, 1=coolant, 2=voltage)
- `_activePid` — which PID is currently being queried

**New public getters:**

- `getSpeed()` → int (km/h)
- `getCoolantTemp()` → int (°C)
- `getVoltage()` → float (V)

**poll() internal logic:**

1. If no query in progress and interval has elapsed, determine next PID:
   - If `_cycleCount % CFG_SLOW_PID_EVERY_N == 0` and `_cycleCount > 0`: pick next slow PID from rotation
   - Otherwise: pick speed
2. Call the appropriate ELMduino method for the active PID
3. On `ELM_SUCCESS`: store value in the correct field, increment cycle counter, return `PollResult::SUCCESS`
4. On error (not `ELM_GETTING_MSG`): increment cycle counter, return `PollResult::ERROR`
5. On `ELM_GETTING_MSG`: return `PollResult::WAITING`

### Error Handling

- The existing consecutive error counter in `main.cpp` continues to work — any PID error increments it, any success resets it
- Individual PID failures don't affect other PIDs' stored values — stale data is shown until the next successful poll
- Initial values before first successful poll: speed=0, fuel=-1 (existing sentinel), coolant=0, voltage=0.0

## Config Changes

### Modified Constants

```cpp
CFG_POLL_INTERVAL_MS = 2000   // changed from 3000
```

### New Constants

```cpp
CFG_SLOW_PID_EVERY_N = 5     // insert a slow PID every Nth poll cycle
```

Font and geometry constants for the speed display will be determined during implementation based on available TFT_eSPI font sizes that best fill the middle zone.

## Memory Impact

No additional sprite memory — still one full-screen sprite. The new stored values in OBDReader add negligible RAM (two ints, one float, two counters). No new libraries required — ELMduino already supports all four PIDs.

## Files Modified

| File | Changes |
|------|---------|
| `config.h` | Poll interval to 2000, add `CFG_SLOW_PID_EVERY_N`, speed display geometry constants |
| `obd_reader.h` | ActivePid enum, new stored values, new getters |
| `obd_reader.cpp` | Multi-PID poll() scheduler, new ELMduino calls |
| `gauge_display.h` | `beginFrame()`, `endFrame()`, `drawSpeed()`, `drawVoltage()`, `drawCoolant()` declarations |
| `gauge_display.cpp` | Implement new draw methods, extract fill/CRT/push from `drawGauge()` into frame methods |
| `main.cpp` | `handleRunning()` calls beginFrame/endFrame and all four draw methods, reads new getters |
