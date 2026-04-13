#include "obd_reader.h"
#include "config.h"

void OBDReader::beginBluetooth() {
    _serialBT.begin(CFG_BT_LOCAL_NAME, true);  // true = master mode
    _serialBT.setPin(CFG_BT_PIN);
}

bool OBDReader::connectBT() {
    Serial.print("Connecting to ");
    Serial.println(CFG_BT_DEVICE_NAME);
    return _serialBT.connect(CFG_BT_DEVICE_NAME);
}

bool OBDReader::initELM() {
    return _elm.begin(_serialBT, false, CFG_ELM_TIMEOUT_MS);
}

ActivePid OBDReader::nextPid() {
    // Every Nth cycle, poll a slow PID instead of speed
    if (_cycleCount > 0 && _cycleCount % CFG_SLOW_PID_EVERY_N == 0) {
        static const ActivePid slowPids[] = {
            ActivePid::FUEL, ActivePid::COOLANT, ActivePid::VOLTAGE
        };
        ActivePid pid = slowPids[_slowPidIndex];
        _slowPidIndex = (_slowPidIndex + 1) % 3;
        return pid;
    }
    return ActivePid::SPEED;
}

PollResult OBDReader::dispatchPid() {
    switch (_activePid) {
        case ActivePid::SPEED: {
            int32_t val = _elm.kph();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _speed = val;
                return PollResult::SUCCESS;
            }
            break;
        }
        case ActivePid::FUEL: {
            float val = _elm.fuelLevel();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _lastFuelLevel = val;
                return PollResult::SUCCESS;
            }
            break;
        }
        case ActivePid::COOLANT: {
            float val = _elm.engineCoolantTemp();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _coolantTemp = (int)val;
                return PollResult::SUCCESS;
            }
            break;
        }
        case ActivePid::VOLTAGE: {
            float val = _elm.ctrlModVoltage();
            if (_elm.nb_rx_state == ELM_SUCCESS) {
                _voltage = val;
                return PollResult::SUCCESS;
            }
            break;
        }
    }

    if (_elm.nb_rx_state != ELM_GETTING_MSG) {
        Serial.print("OBD error: ");
        _elm.printError();
        return PollResult::ERROR;
    }

    return PollResult::WAITING;
}

PollResult OBDReader::poll() {
    if (!_queryInProgress) {
        if (millis() - _lastPollTime < CFG_POLL_INTERVAL_MS) {
            return PollResult::WAITING;
        }
        _activePid = nextPid();
        _queryInProgress = true;
    }

    PollResult result = dispatchPid();

    if (result != PollResult::WAITING) {
        _queryInProgress = false;
        _lastPollTime = millis();
        _cycleCount++;
    }

    return result;
}

float OBDReader::getFuelLevel() {
    return _lastFuelLevel;
}

int OBDReader::getSpeed() {
    return _speed;
}

int OBDReader::getCoolantTemp() {
    return _coolantTemp;
}

float OBDReader::getVoltage() {
    return _voltage;
}

bool OBDReader::isConnected() {
    return _serialBT.connected();
}

int OBDReader::getError() {
    return _elm.nb_rx_state;
}
