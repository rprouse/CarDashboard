#pragma once

#include <TFT_eSPI.h>

class GaugeDisplay {
public:
    void begin(TFT_eSPI& tft);
    void drawConnecting();
    void drawInitialising();
    void drawGauge(float fuelPercent);
    void drawError(const char* msg);

private:
    void applyCrtEffect();

    TFT_eSprite* _sprite = nullptr;
    TFT_eSPI*    _tft    = nullptr;
};
