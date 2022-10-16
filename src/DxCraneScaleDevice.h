//
// Created by Andri Yadi on 22/09/22.
//

#ifndef BIOFLOCFEEDER_DXCRANESCALEDEVICE_H
#define BIOFLOCFEEDER_DXCRANESCALEDEVICE_H

#include "Arduino.h"
#include "DxCraneScaleBLEClient.h"

#define CRANESCALE_RESPECT_MEASUREMENT_MODE     0//1

enum DxCraneScaleDeviceState_e {
    DxCraneScaleDeviceState_Off = 0,
    DxCraneScaleDeviceState_On,
    DxCraneScaleDeviceState_OnAndIdle,
    DxCraneScaleDeviceState_Found,
    DxCraneScaleDeviceState_Measuring
};

struct DxCraneScaleDeviceTimeTracker_t {
    unsigned long lastDeviceActivity {0};
};

class DxCraneScaleDevice {
public:
    explicit DxCraneScaleDevice(uint8_t powerPin, uint8_t holdPin);
    explicit DxCraneScaleDevice(uint8_t powerPin, uint8_t holdPin, uint8_t actPin);
    ~DxCraneScaleDevice();

    bool begin();
    bool waitForConnected(uint32_t timeout=0);
    bool run();

    void turnOn();
    void turnOff();
    void unhold() const;
    bool isBacklightActive();

    void sleepBacklight(bool recheck=false);

#if CRANESCALE_RESPECT_MEASUREMENT_MODE
    bool startMeasurement(bool waitForConnected = false);
    bool stopMeasurement();
#endif

    bool isDeviceFound() const {
        return (currentState_ >= DxCraneScaleDeviceState_Found);
    }

    DxCraneScaleBLEClientValues_t &getLastValues() {
        return client_->getLastValues();
    }

    uint8_t powerPinValueIfAsInput = HIGH;
    void setPowerButtonClicked();

protected:
    uint8_t powerPin_ {255};
    uint8_t holdPin_ {255};
    uint8_t activeInterruptPin_ {255};

    DxCraneScaleDeviceState_e currentState_ = DxCraneScaleDeviceState_Off;
    void setCurrentState(DxCraneScaleDeviceState_e state);

    // How long to wait before it's assumed device is OFF
    unsigned long noActivityTimeoutMS_ = 10000;

#if CRANESCALE_RESPECT_MEASUREMENT_MODE
    bool measurementStarted_ = false;
#endif

    DxCraneScaleDeviceTimeTracker_t timeTracker_ {};

    DxCraneScaleBLEIdentifier clientId_;
    DxCraneScaleBLEClient *client_ {nullptr};
    void onClientEvent(const DxCraneScaleBLEClientValues_t &values, DxCraneScaleBLEClientState_e state);

    void checkActivityToMarkOff();

    void changePowerPinMode(uint8_t mode, bool detachInt=true);
    unsigned long lastPowerPinStateChanged_ = 0;
    bool powerButtonIsClicked_ = false;

    void changeActivePinMode(uint8_t mode, bool detachInt=true);
};


#endif //BIOFLOCFEEDER_DXCRANESCALEDEVICE_H
