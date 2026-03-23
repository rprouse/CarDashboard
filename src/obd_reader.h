#pragma once

#include "BluetoothSerial.h"
#include "ELMduino.h"

enum class PollResult { SUCCESS, WAITING, ERROR };

class OBDReader {
public:
    void beginBluetooth();
    bool connectBT();
    bool initELM();
    PollResult poll();
    float getFuelLevel();
    bool isConnected();
    int getError();

private:
    BluetoothSerial _serialBT;
    ELM327 _elm;
    float _lastFuelLevel = -1.0f;
    bool _queryInProgress = false;
    unsigned long _lastPollTime = 0;
};
