#pragma once

#include "BluetoothSerial.h"
#include "ELMduino.h"
#include "config.h"

enum class PollResult { SUCCESS, WAITING, ERROR };

enum class ActivePid { SPEED, FUEL_RATE, FUEL, COOLANT, VOLTAGE };

class OBDReader {
public:
    void beginBluetooth();
    bool connectBT();
    bool initELM();
    PollResult poll();

    float getFuelLevel();
    int   getSpeed();
    int   getCoolantTemp();
    float getVoltage();
    float getFuelRate();

    bool isConnected();
    int  getError();

private:
    BluetoothSerial _serialBT;
    ELM327 _elm;

    // Last-known PID values
    float _lastFuelLevel  = -1.0f;
    int   _speed          = 0;
    int   _coolantTemp    = 0;
    float _voltage        = 0.0f;

    // Fuel rate rolling mean
    float _fuelRateBuf[CFG_FUELRATE_SMOOTH_SAMPLES] = {0};
    int   _fuelRateBufIdx   = 0;
    int   _fuelRateBufCount = 0;

    // Poll scheduler state
    ActivePid _activePid       = ActivePid::SPEED;
    bool      _queryInProgress = false;
    unsigned long _lastPollTime = 0;
    int       _cycleCount      = 0;
    int       _slowPidIndex    = 0;  // 0=fuel, 1=coolant, 2=voltage
    int       _fastPidIndex    = 0;  // 0=speed, 1=fuel_rate

    ActivePid nextPid();
    PollResult dispatchPid();
};
