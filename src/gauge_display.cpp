#include "gauge_display.h"
#include "config.h"

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
    _sprite->setTextColor(TFT_WHITE);
    _sprite->setTextFont(4);
    _sprite->drawString("CONNECTING...", CFG_SCREEN_W / 2, CFG_SCREEN_H / 2);
    _sprite->pushSprite(0, 0);
}

void GaugeDisplay::drawInitialising() {
    if (!_sprite) return;

    _sprite->fillSprite(TFT_BLACK);
    _sprite->setTextColor(TFT_WHITE);
    _sprite->setTextFont(4);
    _sprite->drawString("INITIALISING OBD...", CFG_SCREEN_W / 2, CFG_SCREEN_H / 2);
    _sprite->pushSprite(0, 0);
}

void GaugeDisplay::drawGauge(float fuelPercent) {
    if (!_sprite) return;

    // Clamp to 0-100
    if (fuelPercent < 0.0f)   fuelPercent = 0.0f;
    if (fuelPercent > 100.0f) fuelPercent = 100.0f;

    _sprite->fillSprite(TFT_BLACK);

    // Outer bar border
    _sprite->drawRect(CFG_BAR_X, CFG_BAR_Y, CFG_BAR_W, CFG_BAR_H, TFT_WHITE);

    // Fill bar
    int fillW = (int)((CFG_BAR_W - 2 * CFG_BAR_PADDING) * fuelPercent / 100.0f);
    uint16_t colour = fuelColour(fuelPercent);
    _sprite->fillRect(CFG_BAR_X + CFG_BAR_PADDING,
                      CFG_BAR_Y + CFG_BAR_PADDING,
                      fillW,
                      CFG_BAR_H - 2 * CFG_BAR_PADDING,
                      colour);

    // Percentage text below bar
    _sprite->setTextColor(TFT_WHITE);
    _sprite->setTextFont(7);
    char buffer[8];
    snprintf(buffer, sizeof(buffer), "%.0f%%", fuelPercent);
    _sprite->drawString(buffer, CFG_SCREEN_W / 2, 160);

    _sprite->pushSprite(0, 0);
}

void GaugeDisplay::drawError(const char* msg) {
    if (!_sprite) return;

    _sprite->fillSprite(TFT_BLACK);
    _sprite->setTextColor(TFT_RED);
    _sprite->setTextFont(4);
    _sprite->drawString(msg, CFG_SCREEN_W / 2, CFG_SCREEN_H / 2);
    _sprite->pushSprite(0, 0);
}

uint16_t GaugeDisplay::fuelColour(float pct) {
    if (pct > CFG_THRESH_GREEN)  return CFG_COLOR_GREEN;
    if (pct > CFG_THRESH_YELLOW) return CFG_COLOR_YELLOW;
    return CFG_COLOR_RED;
}
