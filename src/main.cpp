#include <Arduino.h>
#include <TFT_eSPI.h>
#include "config.h"
#include "state_machine.h"
#include "gauge_display.h"
#include "obd_reader.h"

TFT_eSPI tft;
GaugeDisplay display;
OBDReader obd;

int consecutiveErrors = 0;
unsigned long stateEntryTime = 0;
bool screenDrawn = false;

void enterState(AppState newState) {
    setState(newState);
    stateEntryTime = millis();
    screenDrawn = false;
    if (newState == AppState::BT_CONNECTING || newState == AppState::RUNNING) {
        consecutiveErrors = 0;
    }
}

void handleBtConnecting() {
    if (!screenDrawn) {
        display.drawConnecting();
        screenDrawn = true;
    }
    // SerialBT.connect() is BLOCKING (~10s)
    // Screen already shows "CONNECTING..." before this call
    if (obd.connectBT()) {
        enterState(AppState::OBD_INIT);
    } else {
        delay(CFG_RECONNECT_DELAY_MS);
    }
}

void handleObdInit() {
    if (!screenDrawn) {
        display.drawInitialising();
        screenDrawn = true;
    }
    // myELM327.begin() is BLOCKING (~5s timeout)
    if (obd.initELM()) {
        enterState(AppState::RUNNING);
    } else {
        Serial.println("ELM327 init failed, reconnecting BT...");
        enterState(AppState::BT_CONNECTING);
    }
}

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
            display.drawGauge(obd.getFuelLevel());
            break;

        case PollResult::ERROR:
            consecutiveErrors++;
            Serial.print("Consecutive errors: ");
            Serial.println(consecutiveErrors);
            if (consecutiveErrors >= CFG_CONSECUTIVE_ERROR_THRESHOLD) {
                enterState(AppState::ERROR);
            }
            break;

        case PollResult::WAITING:
            break;
    }

    // Redraw periodically for animations (pulse, flicker)
    static unsigned long lastRedraw = 0;
    if (obd.getFuelLevel() >= 0 && millis() - lastRedraw >= CFG_ANIM_INTERVAL_MS) {
        display.drawGauge(obd.getFuelLevel());
        lastRedraw = millis();
    }
}

void handleError() {
    if (!screenDrawn) {
        display.drawError("NO SIGNAL");
        screenDrawn = true;
    }
    if (millis() - stateEntryTime >= CFG_RECONNECT_DELAY_MS) {
        enterState(AppState::BT_CONNECTING);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("FuelGauge starting...");

    tft.init();
    tft.setRotation(1);
    digitalWrite(TFT_BL, HIGH);
    pinMode(TFT_BL, OUTPUT);

    display.begin(tft);
    display.drawConnecting();

    obd.beginBluetooth();

    enterState(AppState::BT_CONNECTING);
}

void loop() {
    switch (getState()) {
        case AppState::BT_CONNECTING: handleBtConnecting(); break;
        case AppState::OBD_INIT:      handleObdInit();      break;
        case AppState::RUNNING:       handleRunning();       break;
        case AppState::ERROR:         handleError();         break;
    }
}
