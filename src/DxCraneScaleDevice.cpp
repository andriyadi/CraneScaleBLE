//
// Created by Andri Yadi on 22/09/22.
//

#include "DxCraneScaleDevice.h"

DxCraneScaleDevice::DxCraneScaleDevice(uint8_t powerPin, uint8_t holdPin): powerPin_(powerPin), holdPin_(holdPin) {

}

DxCraneScaleDevice::DxCraneScaleDevice(uint8_t powerPin, uint8_t holdPin, uint8_t actPin): powerPin_(powerPin), holdPin_(holdPin), activeInterruptPin_(actPin) {

}

DxCraneScaleDevice::~DxCraneScaleDevice() = default;

static void activePinInterrupted(void *arg) {
    //isr_log_i("Crane scale is active!");
}

bool DxCraneScaleDevice::begin() {

    changePowerPinMode(OUTPUT, false);
    digitalWrite(powerPin_, HIGH);

    delay(100);
    // Change as input right away, to capture OFF
    changePowerPinMode(INPUT_PULLUP);

    delay(100);
//    pinMode(holdPin_, OUTPUT);
//    digitalWrite(holdPin_, HIGH);
    pinMode(holdPin_, INPUT); // Change hold pin as input, so tare button will work

    if (activeInterruptPin_ != 255) {
        changeActivePinMode(INPUT_PULLDOWN);
        //sleepBacklight(true);
        log_i("Backlight active: %d", isBacklightActive());
    }

    if (client_ == nullptr) {
        client_ = new DxCraneScaleBLEClient(clientId_);

        using namespace std::placeholders;
        client_->onEvent(std::bind(&DxCraneScaleDevice::onClientEvent, this, _1, _2));
        // Begin and will start scan first
        client_->begin();
    }

    // First, assumed the device is OFF
    setCurrentState(DxCraneScaleDeviceState_Off);
    // Set to low number, since if device is already ON, should get response faster than that.
    noActivityTimeoutMS_ = 8000;

    return false;
}

bool DxCraneScaleDevice::waitForConnected(uint32_t timeout) {

    auto _start = millis();
    while (!isDeviceFound() && (millis() - _start) < timeout) {
        delay(30);

        // Just call run() to keep respecting state machine
        run();
    }

    return (isDeviceFound());
}

bool DxCraneScaleDevice::run() {

    if (powerButtonIsClicked_) {
        powerButtonIsClicked_ = false;

        if (currentState_ >= DxCraneScaleDeviceState_On) {
            // Assumed device was just off manually by somebody
            setCurrentState(DxCraneScaleDeviceState_Off);
            // Set to low number, since if device is already ON, should get response faster than that.
            // So 1000 ms from now, device will be turned on by code
            noActivityTimeoutMS_ = 5000;
        }
        // Else it's OFF -> Will be turned ON below
    }

    // If device is assumed OFF
    if (currentState_ == DxCraneScaleDeviceState_Off) {

        // And if after certain time stil not yet found, possibly truly off
        if (timeTracker_.lastDeviceActivity > 0 &&
            (millis() - timeTracker_.lastDeviceActivity) > noActivityTimeoutMS_) {
            // So, let's turn it on
            log_i("Possibly off");
            // Will set state to CraneScaleDeviceState_On
            turnOn();
        }
    }

    // If already turned on and stays on "ON" state
    if (currentState_ == DxCraneScaleDeviceState_On) {
        // To start scanning in order to get any response -> So that the state will be changed to 'found' later.
        client_->run();

        // But if still no activity after certain time, possible still off
        checkActivityToMarkOff();
    }

//    // If already turned on and stays on "ON" state
//    if (currentState_ == CraneScaleDeviceState_Found || currentState_ == CraneScaleDeviceState_Measuring) {
//        // But if still no activity after certain time, possible still off
//        checkActivityToMarkOff();
//    }

#if CRANESCALE_RESPECT_MEASUREMENT_MODE

//    // If already turned on and stays on "ON" state
//    if (currentState_ == CraneScaleDeviceState_Found || currentState_ == CraneScaleDeviceState_Measuring) {
//        // But if still no activity after certain time, possible still off
//        checkActivityToMarkOff();
//    }

    // Only run client continously if measurement is started
    if (measurementStarted_) {
        return client_->run();
    }

    return true;

#else
    return client_->run();
#endif
}

void DxCraneScaleDevice::checkActivityToMarkOff() {
    if (timeTracker_.lastDeviceActivity > 0 &&
        (millis() - timeTracker_.lastDeviceActivity) > noActivityTimeoutMS_) {
        // So mark it OFF --> Will be turned ON later
        setCurrentState(DxCraneScaleDeviceState_Off);
        noActivityTimeoutMS_ = 5000;

        log_i("MARKED AS OFF");
    }
}

void DxCraneScaleDevice::onClientEvent(const DxCraneScaleBLEClientValues_t &values, DxCraneScaleBLEClientState_e state) {
    //log_i("Client state: %d", state);

    if (values.holdStatus == DxCraneScaleBLEClientHoldStatus_Hold) {
#if CRANESCALE_RESPECT_MEASUREMENT_MODE
        if (measurementStarted_) {
            unhold();
        } else {
            // else ignore, means stays in hold status
            log_i("In HOLD!");
        }
#else
        unhold();
#endif
    }

    if (state == DxCraneScaleBLEClientState_Found) {
        setCurrentState(DxCraneScaleDeviceState_Found);
    }
    else if (state == DxCraneScaleBLEClientState_Measuring || state == DxCraneScaleBLEClientState_MeasurementHold) {
        setCurrentState(DxCraneScaleDeviceState_Measuring);

#if CRANESCALE_RESPECT_MEASUREMENT_MODE
        // If not in measurement mode, then simply turn off measurement, hence turning off scanning
        if (!measurementStarted_) {
            stopMeasurement();
        }
#endif
    }
    else {
        Serial.print(".");
        timeTracker_.lastDeviceActivity = millis();
    }
}

void DxCraneScaleDevice::unhold() const {
    if (holdPin_ == 255) { return;}

    log_i("UNHOLD....");
    // Make as output to control
    pinMode(holdPin_, OUTPUT);
    digitalWrite(holdPin_, LOW);
    delay(80);
    digitalWrite(holdPin_, HIGH);

    // Make as input again for tare to work
    delay(80);
    pinMode(holdPin_, INPUT);
}

void DxCraneScaleDevice::turnOn() {
    if (powerPin_ == 255) { return;}
    if (currentState_ >= DxCraneScaleDeviceState_On) { return; }

    changePowerPinMode(OUTPUT);

    log_i("TURN ON....");
    digitalWrite(powerPin_, LOW);
    delay(80);
    digitalWrite(powerPin_, HIGH);
    //delay(1000);

    setCurrentState(DxCraneScaleDeviceState_On);

    // Wait longest, as need time for device is really turned on and start broadcasting
    noActivityTimeoutMS_ = 15*1000;

    // Change power pin mode as input right away
    changePowerPinMode(INPUT_PULLUP);

    // Sleep backlight right away
    sleepBacklight();
}

void DxCraneScaleDevice::turnOff() {
    if (powerPin_ == 255) { return;}
    if (currentState_ == DxCraneScaleDeviceState_Off) { return; }

    changePowerPinMode(OUTPUT);

    log_i("TURN OFF....");
    digitalWrite(powerPin_, LOW);
    delay(80);
    digitalWrite(powerPin_, HIGH);

    setCurrentState(DxCraneScaleDeviceState_Off);

    // Change mode as input right away
    changePowerPinMode(INPUT_PULLUP);
}

void DxCraneScaleDevice::tare() {
    // Tare essentially is OFF and ON
    turnOff();
    delay(500);
    turnOn();
}

#if CRANESCALE_RESPECT_MEASUREMENT_MODE
bool CraneScaleDevice::startMeasurement(bool shouldWaitForConnected) {

    // Start scanning to find device and get measurement data
#if CRANESCALE_BLECLIENT_SCAN_FOREVER
    client_->scanForever();
#else
    client_->scan();
#endif
    measurementStarted_ = true;

    bool _connected;
    if (shouldWaitForConnected) {

        _connected = waitForConnected(30000);
        if (_connected) {
            log_i("Scale device is connected, measurement is started");
        }
    }
    else {
        log_i("Measurement is started");
        _connected = false; // as not waiting, assumed device is NOT connected yet.
    }

    //measurementStarted_ = true;

    return _connected;
}

bool CraneScaleDevice::stopMeasurement() {
//    if (currentState_ < CraneScaleDeviceState_Measuring) {
//        log_i("Not measuring");
//        return false;
//    }

    measurementStarted_ = false;

    // Demote state to IDLE, as won't measuring after this
    setCurrentState(CraneScaleDeviceState_OnAndIdle);

    log_i("Measurement is stopped");
    return client_->stopScan();
}
#endif //CRANESCALE_RESPECT_MEASUREMENT_MODE

static void powerPinInterrupted(void *arg) {
    auto *_self = static_cast<DxCraneScaleDevice*>(arg);
    _self->setPowerButtonClicked();
}

void DxCraneScaleDevice::changePowerPinMode(uint8_t mode, bool detachInt) {
    //log_i("Change power in mode to %d", mode);
    if (powerPin_ == 255) {
        return;
    }

    if (mode == INPUT_PULLUP) {
        pinMode(powerPin_, INPUT_PULLUP);
        // Attach interrupt
        attachInterruptArg(powerPin_, powerPinInterrupted, this, FALLING);
    }
    else if (mode == OUTPUT) {
        // Remove interrupt
        if (detachInt) detachInterrupt(powerPin_);
        pinMode(powerPin_, OUTPUT);
    }
}

void DxCraneScaleDevice::setPowerButtonClicked() {
    // Handle debouncing
    if (millis() - lastPowerPinStateChanged_ > 60) {
        powerButtonIsClicked_ = true;
        lastPowerPinStateChanged_ = millis();
    }
}

void DxCraneScaleDevice::setCurrentState(DxCraneScaleDeviceState_e state) {
    timeTracker_.lastDeviceActivity = millis();

    if (state == currentState_) {
        return;
    }

    currentState_ = state;

    if (currentState_ == DxCraneScaleDeviceState_Off) {
        log_i("State: OFF");
    }
    else if (currentState_ == DxCraneScaleDeviceState_On) {
        log_i("State: ON");
    }
    else if (currentState_ == DxCraneScaleDeviceState_OnAndIdle) {
        log_i("State: IDLE");
    }
    else if (currentState_ == DxCraneScaleDeviceState_Found) {
        log_i("State: FOUND");
    }
    else if (currentState_ == DxCraneScaleDeviceState_Measuring) {
        log_i("State: MEASURING");
    }
    else {
        log_i("Current state: %d", currentState_);
    }
}

bool DxCraneScaleDevice::isBacklightActive() {
    if (activeInterruptPin_ == 255) {
        return false;
    }

    //changeActivePinMode(INPUT);
    return (digitalRead(activeInterruptPin_) == HIGH);
}

bool DxCraneScaleDevice::sleepBacklight(bool recheckBacklight, bool forceTurnOff) {
    if (activeInterruptPin_ == 255) {
        return false;
    }

    // Change mode to OUTPUT to try to turn screen off
    changeActivePinMode(OUTPUT, false);
    digitalWrite(activeInterruptPin_, LOW);

//    delay(100);
//    changeActivePinMode(INPUT_PULLDOWN);

    // If recheck flag is true, keep checking backlight status until timeout
    if (recheckBacklight) {
        //pinMode(activeInterruptPin_, INPUT);
        unsigned long _start = millis();
        bool _timeout = false;
        while (isBacklightActive()) {
            if (millis() - _start > 30000) {
                _timeout = true;
                break;
            }
            Serial.print(".");
            delay(200);
            //sleepBacklight();

            // Need to keep run to keep track the state
            run();
        }

        Serial.print("\r\n");
        if (isBacklightActive() && _timeout) {
            if (forceTurnOff) {
                // Just turn off, if requred. Later, device will be turned on automatically when wake up
                turnOff();
                return true;
            }
            else {
                return false;
            }
        }
    }

    return !isBacklightActive();
}

void DxCraneScaleDevice::changeActivePinMode(uint8_t mode, bool detachInt) {
    if (activeInterruptPin_ == 255) {
        return;
    }

    if (mode == INPUT_PULLDOWN || mode == INPUT_PULLUP || mode == INPUT) {
        pinMode(activeInterruptPin_, mode);
        // Attach interrupt
        attachInterruptArg(activeInterruptPin_, activePinInterrupted, this, RISING);
    }
    else if (mode == OUTPUT) {
        // Remove interrupt
        if (detachInt) detachInterrupt(activeInterruptPin_);
        pinMode(activeInterruptPin_, mode);
    }
}





