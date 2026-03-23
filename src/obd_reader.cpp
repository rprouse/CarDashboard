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

PollResult OBDReader::poll() {
    if (!_queryInProgress) {
        if (millis() - _lastPollTime < CFG_POLL_INTERVAL_MS) {
            return PollResult::WAITING;
        }
        _queryInProgress = true;
    }

    float val = _elm.fuelLevel();

    if (_elm.nb_rx_state == ELM_SUCCESS) {
        _lastFuelLevel = val;
        _queryInProgress = false;
        _lastPollTime = millis();
        return PollResult::SUCCESS;
    }

    if (_elm.nb_rx_state != ELM_GETTING_MSG) {
        _queryInProgress = false;
        _lastPollTime = millis();
        Serial.print("OBD error: ");
        _elm.printError();
        return PollResult::ERROR;
    }

    return PollResult::WAITING;
}

float OBDReader::getFuelLevel() {
    return _lastFuelLevel;
}

bool OBDReader::isConnected() {
    return _serialBT.connected();
}

int OBDReader::getError() {
    return _elm.nb_rx_state;
}
