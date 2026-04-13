#include "gauge_display.h"
#include "config.h"
#include <math.h>

void GaugeDisplay::begin(TFT_eSPI& tft) {
    _tft = &tft;

    _sprite = new TFT_eSprite(&tft);

    // Try 16-bit colour depth first
    _sprite->setColorDepth(16);
    if (_sprite->createSprite(CFG_SCREEN_W, CFG_SCREEN_H) == nullptr) {
        // Fall back to 8-bit on OOM
        _sprite->setColorDepth(8);
        if (_sprite->createSprite(CFG_SCREEN_W, CFG_SCREEN_H) == nullptr) {
            Serial.println("ERROR: Could not allocate sprite at any colour depth");
            return;
        }
        Serial.println("Sprite created at 8-bit colour depth (fallback)");
    }

    _sprite->setTextDatum(MC_DATUM);
}

void GaugeDisplay::drawConnecting() {
    if (!_sprite) return;

    _sprite->fillSprite(TFT_BLACK);
    _sprite->setTextColor(CFG_COLOR_CRT_GREEN);
    _sprite->setTextFont(4);
    _sprite->drawString("CONNECTING...", CFG_SCREEN_W / 2, CFG_SCREEN_H / 2);
    applyCrtEffect();
    _sprite->pushSprite(0, 0);
}

void GaugeDisplay::drawInitialising() {
    if (!_sprite) return;

    _sprite->fillSprite(TFT_BLACK);
    _sprite->setTextColor(CFG_COLOR_CRT_GREEN);
    _sprite->setTextFont(4);
    _sprite->drawString("INITIALISING OBD...", CFG_SCREEN_W / 2, CFG_SCREEN_H / 2);
    applyCrtEffect();
    _sprite->pushSprite(0, 0);
}

void GaugeDisplay::beginFrame() {
    if (!_sprite) return;
    _sprite->fillSprite(TFT_BLACK);
}

void GaugeDisplay::endFrame() {
    if (!_sprite) return;
    applyCrtEffect();
    _sprite->pushSprite(0, 0);
}

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

    return y + _sprite->fontHeight() + 4; // Return Y position below label for next element
}

void GaugeDisplay::drawError(const char* msg) {
    if (!_sprite) return;

    _sprite->fillSprite(TFT_BLACK);
    _sprite->setTextColor(CFG_COLOR_CRT_GREEN);
    _sprite->setTextFont(4);
    _sprite->drawString(msg, CFG_SCREEN_W / 2, CFG_SCREEN_H / 2);
    applyCrtEffect();
    _sprite->pushSprite(0, 0);
}

void GaugeDisplay::applyCrtEffect() {
    // Scanlines: fixed intensity
    for (int y = 0; y < CFG_SCREEN_H; y += CFG_SCANLINE_SPACING) {
        _sprite->drawFastHLine(0, y, CFG_SCREEN_W, CFG_COLOR_SCANLINE);
    }
}

