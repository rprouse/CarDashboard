#pragma once

enum class AppState {
    BT_CONNECTING,
    OBD_INIT,
    RUNNING,
    ERROR
};

void       setState(AppState newState);
AppState   getState();
const char* getStateName(AppState state);
