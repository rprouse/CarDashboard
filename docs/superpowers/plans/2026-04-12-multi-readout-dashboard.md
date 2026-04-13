# Multi-Readout Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expand the fuel-only gauge into a four-readout dashboard showing fuel level, vehicle speed, battery voltage, and coolant temperature.

**Architecture:** Multi-PID scheduler in OBDReader multiplexes four PIDs over the ELM327 connection. GaugeDisplay gains beginFrame/endFrame for compositing four draw methods into a single sprite push. Speed polls every 2s; fuel, coolant, and voltage rotate every 5th cycle (~30s each).

**Tech Stack:** ESP32/Arduino, TFT_eSPI (sprites, Font 4 + Font 7), ELMduino (non-blocking PID queries), BluetoothSerial

**Spec:** `docs/superpowers/specs/2026-04-12-multi-readout-dashboard-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/config.h` | Modify | Add poll interval change, slow PID constant, speed display geometry |
| `src/obd_reader.h` | Modify | Add ActivePid enum, new stored values, new getters |
| `src/obd_reader.cpp` | Modify | Multi-PID poll() scheduler |
| `src/gauge_display.h` | Modify | Add beginFrame, endFrame, drawSpeed, drawVoltage, drawCoolant declarations |
| `src/gauge_display.cpp` | Modify | Implement new draw methods, extract pipeline from drawGauge |
| `src/main.cpp` | Modify | Compose all four readouts in handleRunning() |

---

### Task 1: Add New Config Constants

**Files:**
- Modify: `src/config.h`

- [ ] **Step 1: Update poll interval and add new constants**

In `src/config.h`, change `CFG_POLL_INTERVAL_MS` from 3000 to 2000, and add the new constants:

```cpp
// ── Timing (ms) ─────────────────────────────────────────────
constexpr unsigned long CFG_POLL_INTERVAL_MS   = 2000;
constexpr unsigned long CFG_RECONNECT_DELAY_MS = 5000;
constexpr unsigned long CFG_ELM_TIMEOUT_MS     = 5000;

// ── OBD polling ─────────────────────────────────────────────
constexpr int CFG_SLOW_PID_EVERY_N = 5;  // insert a slow PID every Nth poll cycle
```

Add speed display geometry constants after the gauge geometry section:

```cpp
// ── Speed display ───────────────────────────────────────────
constexpr int CFG_SPEED_FONT       = 7;   // 7-segment font for digits
constexpr int CFG_SPEED_FONT_SIZE  = 2;   // text size multiplier (48px × 2 = 96px)
constexpr int CFG_LABEL_FONT       = 4;   // font for km/h, voltage, coolant labels
```

- [ ] **Step 2: Build to verify no errors**

Run: `pio run`
Expected: Successful compilation with no errors.

- [ ] **Step 3: Commit**

```bash
git add src/config.h
git commit -m "feat: add config constants for multi-PID polling and speed display"
```

---

### Task 2: Extend OBDReader with Multi-PID State

**Files:**
- Modify: `src/obd_reader.h`

- [ ] **Step 1: Add ActivePid enum and new members to OBDReader**

Replace the entire contents of `src/obd_reader.h` with:

```cpp
#pragma once

#include "BluetoothSerial.h"
#include "ELMduino.h"

enum class PollResult { SUCCESS, WAITING, ERROR };

enum class ActivePid { SPEED, FUEL, COOLANT, VOLTAGE };

class OBDReader {
public:
    void beginBluetooth();
    bool connectBT();
    bool initELM();
    PollResult poll();

    float getFuelLevel();
    int   getSpeed();
    int   getCoolantTemp();
    float getVoltage();

    bool isConnected();
    int  getError();

private:
    BluetoothSerial _serialBT;
    ELM327 _elm;

    // Last-known PID values
    float _lastFuelLevel  = -1.0f;
    int   _speed          = 0;
    int   _coolantTemp    = 0;
    float _voltage        = 0.0f;

    // Poll scheduler state
    ActivePid _activePid       = ActivePid::SPEED;
    bool      _queryInProgress = false;
    unsigned long _lastPollTime = 0;
    int       _cycleCount      = 0;
    int       _slowPidIndex    = 0;  // 0=fuel, 1=coolant, 2=voltage

    ActivePid nextPid();
    PollResult dispatchPid();
};
```

- [ ] **Step 2: Build to verify header compiles**

Run: `pio run`
Expected: Compilation will fail because `obd_reader.cpp` doesn't implement the new methods yet. That's expected — we'll fix it in Task 3. Verify the error is only about missing definitions, not syntax errors in the header.

---

### Task 3: Implement Multi-PID Poll Scheduler

**Files:**
- Modify: `src/obd_reader.cpp`

- [ ] **Step 1: Replace obd_reader.cpp with multi-PID implementation**

Replace the entire contents of `src/obd_reader.cpp` with:

```cpp
#include "obd_reader.h"
#include "config.h"

void OBDReader::beginBluetooth() {
    _serialBT.begin(CFG_BT_LOCAL_NAME, true);  // true = master mode
    _serialBT.setPin(CFG_BT_PIN);
}

bool OBDReader::connectBT() {
    Serial.print("Connecting to ");
    Serial.println(CFG_BT_DEVICE_NAME);
    return _serialBT.connect(CFG_BT_DEVICE_NAME);
}

bool OBDReader::initELM() {
    return _elm.begin(_serialBT, false, CFG_ELM_TIMEOUT_MS);
}

ActivePid OBDReader::nextPid() {
    // Every Nth cycle, poll a slow PID instead of speed
    if (_cycleCount > 0 && _cycleCount % CFG_SLOW_PID_EVERY_N == 0) {
        static const ActivePid slowPids[] = {
            ActivePid::FUEL, ActivePid::COOLANT, ActivePid::VOLTAGE
        };
        ActivePid pid = slowPids[_slowPidIndex];
        _slowPidIndex = (_slowPidIndex + 1) % 3;
        return pid;
    }
    return ActivePid::SPEED;
}

PollResult OBDReader::dispatchPid() {
    switch (_activePid) {
        case ActivePid::SPEED: {
            int32_t val = _elm.kph();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _speed = val;
                return PollResult::SUCCESS;
            }
            break;
        }
        case ActivePid::FUEL: {
            float val = _elm.fuelLevel();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _lastFuelLevel = val;
                return PollResult::SUCCESS;
            }
            break;
        }
        case ActivePid::COOLANT: {
            float val = _elm.engineCoolantTemp();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _coolantTemp = (int)val;
                return PollResult::SUCCESS;
            }
            break;
        }
        case ActivePid::VOLTAGE: {
            float val = _elm.ctrlModVoltage();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _voltage = val;
                return PollResult::SUCCESS;
            }
            break;
        }
    }

    if (_elm.nb_rx_state != ELM_GETTING_MSG) {
        Serial.print("OBD error: ");
        _elm.printError();
        return PollResult::ERROR;
    }

    return PollResult::WAITING;
}

PollResult OBDReader::poll() {
    if (!_queryInProgress) {
        if (millis() - _lastPollTime < CFG_POLL_INTERVAL_MS) {
            return PollResult::WAITING;
        }
        _activePid = nextPid();
        _queryInProgress = true;
    }

    PollResult result = dispatchPid();

    if (result != PollResult::WAITING) {
        _queryInProgress = false;
        _lastPollTime = millis();
        _cycleCount++;
    }

    return result;
}

float OBDReader::getFuelLevel() {
    return _lastFuelLevel;
}

int OBDReader::getSpeed() {
    return _speed;
}

int OBDReader::getCoolantTemp() {
    return _coolantTemp;
}

float OBDReader::getVoltage() {
    return _voltage;
}

bool OBDReader::isConnected() {
    return _serialBT.connected();
}

int OBDReader::getError() {
    return _elm.nb_rx_state;
}
```

- [ ] **Step 2: Build to verify compilation**

Run: `pio run`
Expected: Successful compilation with no errors.

- [ ] **Step 3: Commit**

```bash
git add src/obd_reader.h src/obd_reader.cpp
git commit -m "feat: add multi-PID poll scheduler for speed, fuel, coolant, voltage"
```

---

### Task 4: Add beginFrame/endFrame and Extract Pipeline from drawGauge

**Files:**
- Modify: `src/gauge_display.h`
- Modify: `src/gauge_display.cpp`

- [ ] **Step 1: Add new method declarations to gauge_display.h**

In `src/gauge_display.h`, add the new public method declarations. The full public interface should be:

```cpp
void begin(TFT_eSPI& tft);

// Frame control
void beginFrame();
void endFrame();

// Content draw methods (call between beginFrame/endFrame)
int32_t drawGauge(float percent, int32_t y);
int32_t drawSpeed(int kph, int32_t y);
int32_t drawVoltage(float volts, int32_t y);
int32_t drawCoolant(int tempC, int32_t y);

// Self-contained screens (own their full pipeline)
void drawConnecting();
void drawInitialising();
void drawError(const char* msg);
```

- [ ] **Step 2: Implement beginFrame and endFrame in gauge_display.cpp**

Add these two methods to `src/gauge_display.cpp`:

```cpp
void GaugeDisplay::beginFrame() {
    if (!_sprite) return;
    _sprite->fillSprite(TFT_BLACK);
}

void GaugeDisplay::endFrame() {
    if (!_sprite) return;
    applyCrtEffect();
    _sprite->pushSprite(0, 0);
}
```

- [ ] **Step 3: Remove fillSprite/applyCrtEffect/pushSprite from drawGauge**

In `src/gauge_display.cpp`, modify `drawGauge()` to remove the three pipeline calls it currently owns. Remove these lines:

- Remove: `_sprite->fillSprite(TFT_BLACK);` (line 54)
- Remove: `applyCrtEffect();` (line 99)
- Remove: `_sprite->pushSprite(0, 0);` (line 100)

The method should now start directly with the percent clamping and end with the `return y + ...` line. It becomes a pure content-drawing method.

The updated `drawGauge` method should look like:

```cpp
int32_t GaugeDisplay::drawGauge(float percent, int32_t y) {
    if (!_sprite) return y;

    // Clamp to 0-100
    if (percent < 0.0f)   percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;

    uint16_t barColour = CFG_COLOR_BAR;

    // Low fuel danger flash overrides pulse colour
    bool dangerFlash = false;
    if (percent < CFG_THRESH_LOW_FUEL) {
      dangerFlash = ((millis() / CFG_DANGER_BLINK_MS) % 2) == 1;
      if (dangerFlash) {
        barColour = CFG_COLOR_DANGER;
      }
    }

    y += 4; // Top Border padding

    // Outer bar border
    _sprite->drawRect(CFG_BAR_X, y, CFG_BAR_W, CFG_BAR_H,
                      dangerFlash ? CFG_COLOR_DANGER : CFG_COLOR_CRT_DIM);

    // Fill bar
    int fillW = (int)((CFG_BAR_W - 2 * CFG_BAR_PADDING) * percent / 100.0f);
    _sprite->fillRect(CFG_BAR_X + CFG_BAR_PADDING,
                      y + CFG_BAR_PADDING,
                      fillW,
                      CFG_BAR_H - 2 * CFG_BAR_PADDING,
                      barColour);

    // Hash marks at 25%, 50%, 75%
    for (int pct = 25; pct <= 75; pct += 25) {
        int markX = CFG_BAR_X + CFG_BAR_PADDING
                    + (int)((CFG_BAR_W - 2 * CFG_BAR_PADDING) * pct / 100.0f);
        _sprite->drawFastVLine(markX, y + CFG_BAR_PADDING,
                               CFG_BAR_H - 2 * CFG_BAR_PADDING, CFG_COLOR_CRT_DIM);
    }

    y += CFG_BAR_H + 4;

    // "Fuel: XX%" label below bar — all Font 4
    _sprite->setTextColor(dangerFlash ? CFG_COLOR_DANGER : CFG_COLOR_BAR);
    _sprite->setTextDatum(TL_DATUM);
    _sprite->setTextFont(4);
    char labelBuf[18];
    snprintf(labelBuf, sizeof(labelBuf), "SYS://FUEL: %.0f%%", percent);
    _sprite->drawString(labelBuf, CFG_BAR_X, y);

    return y + _sprite->fontHeight() + 4;
}
```

- [ ] **Step 4: Build to verify compilation**

Run: `pio run`
Expected: Successful compilation. The new `drawSpeed`, `drawVoltage`, `drawCoolant` methods are declared but not yet defined — this will cause a linker error only if they're called. Since `main.cpp` doesn't call them yet, build should succeed.

- [ ] **Step 5: Commit**

```bash
git add src/gauge_display.h src/gauge_display.cpp
git commit -m "refactor: extract frame pipeline from drawGauge into beginFrame/endFrame"
```

---

### Task 5: Implement drawSpeed

**Files:**
- Modify: `src/gauge_display.cpp`

- [ ] **Step 1: Add drawSpeed method**

Add this method to `src/gauge_display.cpp`:

```cpp
int32_t GaugeDisplay::drawSpeed(int kph, int32_t y) {
    if (!_sprite) return y;

    // Clamp to 0-999
    if (kph < 0)   kph = 0;
    if (kph > 999) kph = 999;

    // Format speed with leading zeros
    char speedBuf[4];
    snprintf(speedBuf, sizeof(speedBuf), "%03d", kph);

    // Draw speed digits in large 7-segment font
    _sprite->setTextColor(CFG_COLOR_CRT_GREEN);
    _sprite->setTextSize(CFG_SPEED_FONT_SIZE);
    _sprite->setTextFont(CFG_SPEED_FONT);
    _sprite->setTextDatum(BL_DATUM);  // baseline-left for alignment

    // Vertically centre the speed in the available space between fuel label and bottom readouts
    // Font 7 at size 2 = 96px tall. Font 4 = 26px tall for bottom row.
    // Available height: 240 - y (top) - 26 (bottom label) - 8 (padding) = remaining
    int speedHeight = 48 * CFG_SPEED_FONT_SIZE;  // Font 7 base height × size
    int bottomRowHeight = 26 + 8;                  // Font 4 height + padding
    int availableHeight = CFG_SCREEN_H - y - bottomRowHeight;
    int speedY = y + (availableHeight + speedHeight) / 2;  // baseline Y for BL_DATUM

    // Measure speed text width to position "km/h" to its right
    int speedWidth = _sprite->textWidth(speedBuf);

    // Centre the combined "008 km/h" block horizontally
    _sprite->setTextFont(CFG_SPEED_FONT);
    _sprite->setTextSize(CFG_SPEED_FONT_SIZE);
    int gapWidth = 6;  // pixels between speed and "km/h"

    // Measure "km/h" width in Font 4 at size 1
    _sprite->setTextSize(1);
    _sprite->setTextFont(CFG_LABEL_FONT);
    int kmhWidth = _sprite->textWidth("km/h");

    int totalWidth = speedWidth + gapWidth + kmhWidth;
    int startX = (CFG_SCREEN_W - totalWidth) / 2;

    // Draw speed digits
    _sprite->setTextFont(CFG_SPEED_FONT);
    _sprite->setTextSize(CFG_SPEED_FONT_SIZE);
    _sprite->setTextDatum(BL_DATUM);
    _sprite->drawString(speedBuf, startX, speedY);

    // Draw "km/h" at same baseline in Font 4
    _sprite->setTextSize(1);
    _sprite->setTextFont(CFG_LABEL_FONT);
    _sprite->setTextDatum(BL_DATUM);
    _sprite->drawString("km/h", startX + speedWidth + gapWidth, speedY);

    // Return Y for bottom row
    return CFG_SCREEN_H - bottomRowHeight;
}
```

- [ ] **Step 2: Build to verify compilation**

Run: `pio run`
Expected: Successful compilation with no errors.

- [ ] **Step 3: Commit**

```bash
git add src/gauge_display.cpp
git commit -m "feat: implement drawSpeed with large 7-segment digits and km/h label"
```

---

### Task 6: Implement drawVoltage and drawCoolant

**Files:**
- Modify: `src/gauge_display.cpp`

- [ ] **Step 1: Add drawVoltage method**

Add this method to `src/gauge_display.cpp`:

```cpp
int32_t GaugeDisplay::drawVoltage(float volts, int32_t y) {
    if (!_sprite) return y;

    _sprite->setTextColor(CFG_COLOR_CRT_GREEN);
    _sprite->setTextFont(CFG_LABEL_FONT);
    _sprite->setTextSize(1);
    _sprite->setTextDatum(TL_DATUM);

    char buf[16];
    snprintf(buf, sizeof(buf), "VOLT://%.1fV", volts);
    _sprite->drawString(buf, CFG_BAR_X, y);

    return y + _sprite->fontHeight() + 4;
}
```

- [ ] **Step 2: Add drawCoolant method**

Add this method to `src/gauge_display.cpp`:

```cpp
int32_t GaugeDisplay::drawCoolant(int tempC, int32_t y) {
    if (!_sprite) return y;

    _sprite->setTextColor(CFG_COLOR_CRT_GREEN);
    _sprite->setTextFont(CFG_LABEL_FONT);
    _sprite->setTextSize(1);
    _sprite->setTextDatum(TR_DATUM);

    char buf[16];
    snprintf(buf, sizeof(buf), "TEMP://%d\xB0""C", tempC);
    _sprite->drawString(buf, CFG_SCREEN_W - CFG_BAR_X, y);

    return y + _sprite->fontHeight() + 4;
}
```

Note: `\xB0` is the degree symbol (°) in the TFT_eSPI character set. The `""` between `\xB0` and `"C"` is string literal concatenation — it prevents the compiler from interpreting `\xB0C` as a single three-character hex escape.

- [ ] **Step 3: Build to verify compilation**

Run: `pio run`
Expected: Successful compilation with no errors.

- [ ] **Step 4: Commit**

```bash
git add src/gauge_display.cpp
git commit -m "feat: implement drawVoltage and drawCoolant bottom-row readouts"
```

---

### Task 7: Compose Full Dashboard in main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Update handleRunning() to compose all four readouts**

Replace the `handleRunning()` function in `src/main.cpp` with:

```cpp
void handleRunning() {
    if (!obd.isConnected()) {
        Serial.println("BT disconnected");
        enterState(AppState::ERROR);
        return;
    }

    PollResult result = obd.poll();

    switch (result) {
        case PollResult::SUCCESS:
            consecutiveErrors = 0;
            break;

        case PollResult::ERROR:
            consecutiveErrors++;
            Serial.print("Consecutive errors: ");
            Serial.println(consecutiveErrors);
            if (consecutiveErrors >= CFG_CONSECUTIVE_ERROR_THRESHOLD) {
                enterState(AppState::ERROR);
                return;
            }
            break;

        case PollResult::WAITING:
            break;
    }

    // Redraw periodically for animations (pulse, danger flash, scanlines)
    static unsigned long lastRedraw = 0;
    if (millis() - lastRedraw >= CFG_ANIM_INTERVAL_MS) {
        display.beginFrame();
        int32_t y = display.drawGauge(obd.getFuelLevel(), CFG_FUELBAR_Y);
        y = display.drawSpeed(obd.getSpeed(), y);
        display.drawVoltage(obd.getVoltage(), y);
        display.drawCoolant(obd.getCoolantTemp(), y);
        display.endFrame();
        lastRedraw = millis();
    }
}
```

Key changes from the original:
- Polling and drawing are decoupled — poll result is handled first, then the frame is drawn unconditionally on the animation timer.
- All four readouts compose into a single frame via beginFrame/endFrame.
- The fuel level sentinel (-1) works fine here — the gauge will show 0% until the first successful fuel poll, since `drawGauge` clamps negative values to 0.

- [ ] **Step 2: Build to verify full compilation**

Run: `pio run`
Expected: Successful compilation with no errors and no warnings (other than the existing TOUCH_CS warning).

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: compose full dashboard with fuel, speed, voltage, and coolant"
```

---

### Task 8: Flash and Verify on Hardware

**Files:** None (testing only)

- [ ] **Step 1: Flash to ESP32**

Run: `pio run -t upload`
Expected: Successful upload to the connected ESP32 CYD.

- [ ] **Step 2: Open serial monitor and verify startup**

Run: `pio device monitor`
Expected: Serial output shows "FuelGauge starting..." followed by Bluetooth connection attempts.

- [ ] **Step 3: Verify display layout**

With the device running (connected to OBD2 adapter or not):
- Fuel bar and "SYS://FUEL" label visible at top
- Large 3-digit speed with "km/h" centred in middle zone
- "VOLT://" on bottom-left and "TEMP://" on bottom-right
- CRT scanlines applied across the entire screen
- Animations running (pulse glow, scanlines) at 50ms interval

- [ ] **Step 4: Verify OBD2 polling (if adapter available)**

With OBD2 adapter connected:
- Speed updates every ~2 seconds
- Fuel, coolant temp, and voltage update on slower rotation
- Low-fuel danger flash still works when fuel < 20%
- Serial monitor shows no excessive OBD errors

- [ ] **Step 5: Commit any layout tweaks**

If any geometry or spacing adjustments were needed during testing, commit them:

```bash
git add -A
git commit -m "fix: adjust dashboard layout spacing from hardware testing"
```
