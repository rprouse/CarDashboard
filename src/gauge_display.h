#pragma once

#include <TFT_eSPI.h>

class GaugeDisplay {
public:
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

private:
    TFT_eSPI* _tft = nullptr;
    TFT_eSprite* _sprite = nullptr;

    void applyCrtEffect();
};
