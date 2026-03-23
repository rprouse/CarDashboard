#pragma once

// ── Bluetooth ────────────────────────────────────────────────
const char* const CFG_BT_DEVICE_NAME  = "OBDII";
const char* const CFG_BT_PIN          = "1234";
const char* const CFG_BT_LOCAL_NAME   = "FuelGauge";

// ── Timing (ms) ─────────────────────────────────────────────
constexpr unsigned long CFG_POLL_INTERVAL_MS   = 3000;
constexpr unsigned long CFG_RECONNECT_DELAY_MS = 5000;
constexpr unsigned long CFG_ELM_TIMEOUT_MS     = 5000;

// ── Error handling ──────────────────────────────────────────
constexpr int CFG_CONSECUTIVE_ERROR_THRESHOLD = 3;

// ── Screen ──────────────────────────────────────────────────
constexpr int CFG_SCREEN_W = 320;
constexpr int CFG_SCREEN_H = 240;

// ── Fuel bar geometry (pixels) ──────────────────────────────
constexpr int CFG_BAR_X       = 28;   // was 20, shifted right by border
constexpr int CFG_BAR_Y       = 75;   // was 70, shifted down slightly
constexpr int CFG_BAR_W       = 264;  // was 280, narrowed by 2*border
constexpr int CFG_BAR_H       = 60;
constexpr int CFG_BAR_PADDING = 2;   // border padding for inner fill

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
