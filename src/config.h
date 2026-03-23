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
constexpr int CFG_BAR_X       = 20;
constexpr int CFG_BAR_Y       = 70;
constexpr int CFG_BAR_W       = 280;
constexpr int CFG_BAR_H       = 60;
constexpr int CFG_BAR_PADDING = 2;   // border padding for inner fill

// ── Colours (RGB565) ────────────────────────────────────────
constexpr uint16_t CFG_COLOR_GREEN  = 0x07E0;
constexpr uint16_t CFG_COLOR_YELLOW = 0xFFE0;
constexpr uint16_t CFG_COLOR_RED    = 0xF800;

// ── Colour thresholds (fuel-level percentage) ───────────────
constexpr int CFG_THRESH_GREEN  = 50;  // > 50 % → green
constexpr int CFG_THRESH_YELLOW = 25;  // 25-50 % → yellow, < 25 % → red
