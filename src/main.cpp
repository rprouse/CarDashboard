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

void setup() {
    Serial.begin(115200);
    Serial.println("FuelGauge starting...");

    // Init display
    tft.init();
    tft.setRotation(1);  // landscape
    digitalWrite(TFT_BL, HIGH);  // backlight on (defined in setup header)
    pinMode(TFT_BL, OUTPUT);

    display.begin(tft);
    display.drawConnecting();

    // Init Bluetooth
    obd.beginBluetooth();

    enterState(AppState::BT_CONNECTING);
}

void loop() {
    switch (getState()) {

        case AppState::BT_CONNECTING: {
            if (!screenDrawn) {
                display.drawConnecting();
                screenDrawn = true;
            }
            // SerialBT.connect() is BLOCKING (~10s)
            // Screen already shows "CONNECTING..." before this call
            if (obd.connectBT()) {
                enterState(AppState::OBD_INIT);
            } else {
                // Wait before retrying
                delay(CFG_RECONNECT_DELAY_MS);
                // Note: delay() is acceptable here because we're stuck
                // waiting for BT anyway, and the screen already shows
                // the right status
            }
            break;
        }

        case AppState::OBD_INIT: {
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
            break;
        }

        case AppState::RUNNING: {
            // Check BT still connected
            if (!obd.isConnected()) {
                Serial.println("BT disconnected");
                enterState(AppState::ERROR);
                break;
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
                    // First draw — show gauge with placeholder or just wait
                    // On first successful poll it will update
                    break;
            }
            break;
        }

        case AppState::ERROR: {
            if (!screenDrawn) {
                display.drawError("NO SIGNAL");
                screenDrawn = true;
            }
            // Wait, then retry from BT_CONNECTING
            if (millis() - stateEntryTime >= CFG_RECONNECT_DELAY_MS) {
                enterState(AppState::BT_CONNECTING);
            }
            break;
        }
    }
}
