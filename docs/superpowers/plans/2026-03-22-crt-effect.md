# CRT Phosphor Effect — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the fuel gauge display a retro CRT monitor aesthetic — monochrome phosphor green, scanlines, and a rounded screen border.

**Architecture:** All three effects are applied in `GaugeDisplay` as post-processing steps on the sprite before `pushSprite()`. A private `applyCrtEffect()` method draws scanlines and the rounded border over whatever content is already in the sprite. Every draw method (connecting, initialising, gauge, error) calls it before pushing. All text and graphics switch from white/multicolour to CRT green shades.

**Tech Stack:** TFT_eSPI sprite primitives (`drawFastHLine`, `fillRoundRect`, `fillRect`)

---

## Design Decisions

### Scanlines
Draw a dark (near-black) horizontal line every 3rd pixel row across the sprite. This creates the visible raster-line gaps of a CRT. Using every 3rd row (not every 2nd) keeps readability high on a 240px-tall display. The scanlines are drawn with `drawFastHLine` in a dim colour rather than pure black, so they darken the content behind them without fully erasing it.

Scanline colour: a very dark green (`0x01A0` — approx `#003300` in RGB565) gives a subtly green-tinted gap rather than pure black, which looks more authentic.

### Monochrome green palette
All text and bar graphics change to `CFG_COLOR_CRT_GREEN` (`0x37E6`). The bar border and status text use a dimmer green (`CFG_COLOR_CRT_DIM`, `0x02A0` — approx `#005500`) for contrast. Error text stays in a dim red-ish tone — on a real CRT this would just be a different intensity, but for readability we'll use a dim amber (`CFG_COLOR_CRT_WARN`, `0xC380` — approx `#C07000`).

### Rounded screen border
Simulate the curved CRT bezel by drawing a black `fillRoundRect` border frame around the entire screen. The approach: fill the sprite edges with black rounded corners. Specifically, draw four black filled rectangles along the edges, then use `fillRoundRect` in the content colour cut out from a full-screen black `fillRoundRect` — actually simpler: draw a full-screen black `fillRoundRect` with a large radius FIRST as background, then the content draws on top of the inner area, then scanlines go on top of everything. But that requires content to know about insets.

**Simpler approach:** After all content is drawn (including scanlines), stamp a border frame on top. The border is four `fillRect` calls for the edges + four `fillCircle` calls for inverted corners would be complex. Simplest: use `drawRoundRect` in a loop to create a thick rounded border:

```
for each thickness pixel (0..CFG_CRT_BORDER-1):
    drawRoundRect(i, i, W-2*i, H-2*i, radius-i, TFT_BLACK)
```

This draws concentric rounded rectangles in black, creating a thick rounded border that overlays whatever is underneath.

Border thickness: 8px. Corner radius: 20px. These are tuneable in config.h.

### Inset adjustment
With an 8px border eating into the display, content should be inset so text and bars don't get clipped by the border. Define `CFG_CRT_INSET` (= border thickness) and adjust bar geometry and text positions to respect it. The fuel bar's X/Y/W positions shift inward. Status text centres within the inset area rather than the full screen.

---

## Files Modified

| File | Changes |
|------|---------|
| `src/config.h` | Add CRT colour constants, border/scanline config, adjust bar geometry for inset |
| `src/gauge_display.h` | Add private `applyCrtEffect()` method declaration |
| `src/gauge_display.cpp` | Implement `applyCrtEffect()`, call it in all draw methods, switch all colours to CRT green palette |

No new files. No interface changes — all public methods stay the same.

---

## Task Breakdown

### Task 1: Add CRT constants to config.h

**Files:**
- Modify: `src/config.h`

- [ ] **Step 1:** Add CRT colour constants:
```cpp
// ── CRT effect ────────────────────────────────────────────────
constexpr uint16_t CFG_COLOR_CRT_DIM     = 0x02A0;  // dim green for borders/labels
constexpr uint16_t CFG_COLOR_CRT_WARN    = 0xC380;  // dim amber for error text
constexpr uint16_t CFG_COLOR_SCANLINE    = 0x01A0;  // very dark green scanline

constexpr int CFG_CRT_BORDER_W      = 8;    // border thickness (pixels)
constexpr int CFG_CRT_CORNER_RADIUS = 20;   // rounded corner radius
constexpr int CFG_SCANLINE_SPACING  = 3;    // draw scanline every Nth row
```

- [ ] **Step 2:** Adjust bar geometry to account for border inset — shift bar inward so it doesn't collide with the 8px border:
```cpp
constexpr int CFG_BAR_X       = 28;   // was 20, shifted right by border
constexpr int CFG_BAR_Y       = 75;   // was 70, shifted down slightly
constexpr int CFG_BAR_W       = 264;  // was 280, narrowed by 2*border
```

- [ ] **Step 3:** Build — `pio run`
- [ ] **Step 4:** Commit: "feat: add CRT effect constants and adjust bar geometry for border inset"

---

### Task 2: Implement applyCrtEffect() and monochrome palette

**Files:**
- Modify: `src/gauge_display.h`
- Modify: `src/gauge_display.cpp`

- [ ] **Step 1:** Add `applyCrtEffect()` declaration to `gauge_display.h`:
```cpp
private:
    TFT_eSprite* _sprite = nullptr;
    TFT_eSPI*    _tft    = nullptr;
    void applyCrtEffect();
```

- [ ] **Step 2:** Implement `applyCrtEffect()` in `gauge_display.cpp`:
```cpp
void GaugeDisplay::applyCrtEffect() {
    // Scanlines: draw a dark horizontal line every Nth row
    for (int y = 0; y < CFG_SCREEN_H; y += CFG_SCANLINE_SPACING) {
        _sprite->drawFastHLine(0, y, CFG_SCREEN_W, CFG_COLOR_SCANLINE);
    }

    // Rounded border: draw concentric rounded rects in black
    for (int i = 0; i < CFG_CRT_BORDER_W; i++) {
        _sprite->drawRoundRect(i, i,
                               CFG_SCREEN_W - 2 * i,
                               CFG_SCREEN_H - 2 * i,
                               CFG_CRT_CORNER_RADIUS - i,
                               TFT_BLACK);
    }
}
```

- [ ] **Step 3:** In every draw method, replace `_sprite->pushSprite(0, 0)` with:
```cpp
    applyCrtEffect();
    _sprite->pushSprite(0, 0);
```
Do this in: `drawConnecting()`, `drawInitialising()`, `drawGauge()`, `drawError()`.

- [ ] **Step 4:** Switch `drawConnecting()` and `drawInitialising()` text colour from `TFT_WHITE` to `CFG_COLOR_CRT_GREEN`.

- [ ] **Step 5:** Switch `drawError()` text colour from `TFT_RED` to `CFG_COLOR_CRT_WARN`.

- [ ] **Step 6:** Switch the fuel bar border in `drawGauge()` from `CFG_COLOR_BAR` to `CFG_COLOR_CRT_DIM` (dimmer green for the outline, bright green for the fill — creates contrast).

- [ ] **Step 7:** Build — `pio run`
- [ ] **Step 8:** Commit: "feat: add CRT scanlines, rounded border, and monochrome green palette"
