#pragma once

// ── Bluetooth ────────────────────────────────────────────────
const char* const CFG_BT_DEVICE_NAME  = "OBDII";
const char* const CFG_BT_PIN          = "1234";
const char* const CFG_BT_LOCAL_NAME   = "FuelGauge";

// ── Timing (ms) ─────────────────────────────────────────────
constexpr unsigned long CFG_POLL_INTERVAL_MS   = 500;
constexpr unsigned long CFG_RECONNECT_DELAY_MS = 5000;
constexpr unsigned long CFG_ELM_TIMEOUT_MS     = 5000;

// ── OBD polling ─────────────────────────────────────────────
constexpr int CFG_SLOW_PID_EVERY_N = 10;  // insert a slow PID every Nth poll cycle (10 × 500ms = 5s)

// ── Error handling ──────────────────────────────────────────
constexpr int CFG_CONSECUTIVE_ERROR_THRESHOLD = 3;

// ── Screen ──────────────────────────────────────────────────
constexpr int CFG_SCREEN_W = 320;
constexpr int CFG_SCREEN_H = 240;

// ── Gauge geometry (pixels) ──────────────────────────────
constexpr int CFG_BAR_X       = 28;
constexpr int CFG_BAR_W       = 264;
constexpr int CFG_BAR_H       = 26;
constexpr int CFG_BAR_PADDING = 2;   // border padding for inner fill

constexpr int CFG_FUELBAR_Y   = 0;

// ── Speed display ───────────────────────────────────────────
constexpr int CFG_SPEED_FONT       = 7;   // 7-segment font for digits
constexpr int CFG_SPEED_FONT_SIZE  = 2;   // text size multiplier (48px × 2 = 96px)
constexpr int CFG_LABEL_FONT       = 4;   // font for km/h, voltage, coolant labels

// ── Colours (RGB565) ────────────────────────────────────────
constexpr uint16_t CFG_COLOR_CRT_GREEN = 0x37E6;  // #33FF33 — CRT phosphor green
constexpr uint16_t CFG_COLOR_BAR       = CFG_COLOR_CRT_GREEN;

// ── CRT effect ────────────────────────────────────────────────
constexpr uint16_t CFG_COLOR_CRT_DIM     = 0x02A0;  // dim green for borders/labels
constexpr uint16_t CFG_COLOR_CRT_WARN    = 0xC380;  // dim amber for error text
constexpr uint16_t CFG_COLOR_SCANLINE    = 0x01A0;  // very dark green scanline

constexpr int CFG_CRT_BORDER_W      = 8;    // border thickness (pixels)
constexpr int CFG_CRT_CORNER_RADIUS = 20;   // rounded corner radius
constexpr int CFG_SCANLINE_SPACING  = 3;    // draw scanline every Nth row

// ── HUD chrome ──────────────────────────────────────────────
constexpr int CFG_HUD_INSET        = 12;   // content inset from screen edge
constexpr int CFG_HUD_HEADER_Y     = 14;   // header text Y position
constexpr int CFG_HUD_RULE_UPPER_Y = 28;   // horizontal rule below header
constexpr int CFG_HUD_RULE_LOWER_Y = 148;  // horizontal rule above fuel label

// ── Low fuel warning ────────────────────────────────────────
constexpr int      CFG_THRESH_LOW_FUEL   = 20;    // below this: danger flash
constexpr uint16_t CFG_COLOR_DANGER      = 0xC000; // dim red for low fuel flash
constexpr unsigned long CFG_DANGER_BLINK_MS = 500; // blink period

// ── Animation ───────────────────────────────────────────────
constexpr unsigned long CFG_PULSE_PERIOD_MS = 2000; // bar glow cycle
constexpr int CFG_GLITCH_CHANCE     = 15;           // 1 in N frames triggers glitch
constexpr unsigned long CFG_ANIM_INTERVAL_MS = 50;  // redraw interval for animations
