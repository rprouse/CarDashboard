# Cyberpunk Visual Effects — Implementation Plan (Final)

> This plan reflects the final implemented state after iterative refinement.

**Goal:** Add cyberpunk/HUD visual effects to the fuel gauge: scanlines, pulsing bar glow, hash marks, HUD header, and a low-fuel danger flash.

**Architecture:** All effects are implemented in `gauge_display.cpp` using existing TFT_eSPI sprite primitives. Time-based effects (pulse, flash) use `millis()`. Config constants in `config.h`. No new files, no interface changes.

**Tech Stack:** TFT_eSPI sprite primitives, `millis()`, `sin()` from `<math.h>`

---

## Implemented Effects

### 1. HUD header
- "SYS://FUEL.MONITOR" drawn in Font 2 (small) at top-left in dim green (`CFG_COLOR_CRT_DIM`).
- Rendered by `drawHudChrome()`, called in all four draw methods after `fillSprite(TFT_BLACK)`.
- Appears on every screen (connecting, initialising, gauge, error).

### 2. Scanlines (fixed intensity)
- Dark green horizontal line (`CFG_COLOR_SCANLINE`) every 3rd pixel row.
- Drawn by `applyCrtEffect()`, called after content but before `pushSprite()`.
- Fixed intensity — no flickering or random variation.

### 3. Pulsing bar glow
- Bar fill colour oscillates between 60–100% brightness on a 2-second sine wave.
- Uses `dimColour()` helper to scale RGB565 channels by brightness factor.
- Requires continuous redraw at ~20fps (50ms interval) to animate smoothly.

### 4. Hash marks
- Thin vertical dim-green lines inside the bar at 25%, 50%, 75%.
- Drawn after the bar fill so they appear on top.

### 5. Low fuel danger flash (< 25%)
- When `fuelPercent < CFG_THRESH_LOW_FUEL`, bar fill, bar border, and text alternate between CRT green and dim red (`CFG_COLOR_DANGER`) every 500ms.
- Overrides the pulsing glow colour when flashing red.

### 6. Animation loop
- `handleRunning()` in `main.cpp` redraws the gauge every `CFG_ANIM_INTERVAL_MS` (50ms) for smooth animation.
- `handleError()` redraws on the same timer for live scanlines.
- `handleBtConnecting()` and `handleObdInit()` redraw once before each blocking call (cannot animate continuously due to blocking BT/ELM327 calls).

---

## Effects Considered but Removed

| Effect | Reason removed |
|--------|---------------|
| Flickering scanlines (random intensity per line) | Too distracting on small display |
| Glitch frames (random bright horizontal bands) | Too distracting |
| Horizontal rules (lines above/below content) | Cluttered the display |
| Rounded CRT border | Removed during earlier iteration |

---

## Files Modified

| File | Changes |
|------|---------|
| `src/config.h` | Added: HUD layout constants, low fuel threshold/colour, animation timing, pulse period |
| `src/gauge_display.h` | Added private: `drawHudChrome()`, `dimColour()` |
| `src/gauge_display.cpp` | Implemented all effects: HUD chrome, scanlines, pulsing glow, hash marks, danger flash, `dimColour()` helper |
| `src/main.cpp` | Added animation timer in `handleRunning()` and `handleError()`. Removed `screenDrawn` flag. |

---

## Config Constants Added

```cpp
// HUD chrome
constexpr int CFG_HUD_INSET        = 12;
constexpr int CFG_HUD_HEADER_Y     = 14;
constexpr int CFG_HUD_RULE_UPPER_Y = 28;   // unused (rules removed)
constexpr int CFG_HUD_RULE_LOWER_Y = 148;  // unused (rules removed)

// Low fuel warning
constexpr int      CFG_THRESH_LOW_FUEL   = 25;
constexpr uint16_t CFG_COLOR_DANGER      = 0xC000;
constexpr unsigned long CFG_DANGER_BLINK_MS = 500;

// Animation
constexpr unsigned long CFG_PULSE_PERIOD_MS  = 2000;
constexpr int CFG_GLITCH_CHANCE              = 15;    // unused (glitch removed)
constexpr unsigned long CFG_ANIM_INTERVAL_MS = 50;
```

## Rendering Pipeline

Every draw method follows this order:
1. `fillSprite(TFT_BLACK)` — clear
2. `drawHudChrome()` — header text
3. Content (status text, gauge bar, error message)
4. `applyCrtEffect()` — scanlines
5. `pushSprite(0, 0)` — push to screen
