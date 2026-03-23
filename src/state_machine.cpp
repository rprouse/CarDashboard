#include "state_machine.h"
#include <Arduino.h>

static AppState currentState = AppState::BT_CONNECTING;

void setState(AppState newState) {
    currentState = newState;
    Serial.print("State: ");
    Serial.println(getStateName(newState));
}

AppState getState() {
    return currentState;
}

const char* getStateName(AppState state) {
    switch (state) {
        case AppState::BT_CONNECTING: return "BT_CONNECTING";
        case AppState::OBD_INIT:      return "OBD_INIT";
        case AppState::RUNNING:       return "RUNNING";
        case AppState::ERROR:         return "ERROR";
        default:                      return "UNKNOWN";
    }
}
