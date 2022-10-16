//
// Created by Andri Yadi on 22/09/22.
//

#ifndef BIOFLOCFEEDER_DXCRANESCALEBLECLIENT_H
#define BIOFLOCFEEDER_DXCRANESCALEBLECLIENT_H

//#include "BLEDevice.h"
#include "NimBLEDevice.h"
#include <functional>

#define CRANESCALE_DEFAULT_BLE_ADDRESS      "ff:ff:ff:ff:ff:ff"
#define CRANESCALE_BLECLIENT_SCAN_FOREVER   0//1        // Using scan forever seems not working reliably

class DxCraneScaleBLEIdentifier {
public:
    explicit DxCraneScaleBLEIdentifier(const std::string& fallbackAddr=CRANESCALE_DEFAULT_BLE_ADDRESS);
    virtual ~DxCraneScaleBLEIdentifier();

    BLEAddress *getFallbackDeviceAddress() { return fallbackDeviceAddress_; }
private:
    BLEAddress *fallbackDeviceAddress_ = nullptr;
};


enum DxCraneScaleBLEClientState_e {
    DxCraneScaleBLEClientState_Unknown = 0,
    DxCraneScaleBLEClientState_Began,
    DxCraneScaleBLEClientState_Scanning,
    DxCraneScaleBLEClientState_Found,
    DxCraneScaleBLEClientState_Measuring,
    DxCraneScaleBLEClientState_MeasurementHold
};

enum DxCraneScaleBLEClientHoldStatus_e {
    DxCraneScaleBLEClientHoldStatus_Unknown = 0,
    DxCraneScaleBLEClientHoldStatus_Measuring,
    DxCraneScaleBLEClientHoldStatus_Hold,
};

struct DxCraneScaleBLEClientTimeTracker_t {
    uint32_t lastScan = 0;
    uint32_t lastReceivedData = 0;
};

struct DxCraneScaleBLEClientValues_t {
    float weight = 0;
    DxCraneScaleBLEClientHoldStatus_e holdStatus {DxCraneScaleBLEClientHoldStatus_Unknown};
};

class DxCraneScaleBLEClient {
public:

    typedef std::function<void(const DxCraneScaleBLEClientValues_t &values, DxCraneScaleBLEClientState_e state)> CraneScaleBLEClientEventCallback;

    explicit DxCraneScaleBLEClient(DxCraneScaleBLEIdentifier &id);
    virtual ~DxCraneScaleBLEClient();

    bool begin();
    bool run();

    BLEScanResults scan();
    bool scanForever();
    bool stopScan();

    DxCraneScaleBLEClientValues_t &getLastValues() { return lastValues_; }
    BLEAddress *getAddress() { return identifiers_.getFallbackDeviceAddress(); }

    void onEvent(CraneScaleBLEClientEventCallback cb) {
        eventCallback_ = std::move(cb);
    }

protected:
    DxCraneScaleBLEIdentifier &identifiers_;

    DxCraneScaleBLEClientTimeTracker_t timeTracker_ {};
    DxCraneScaleBLEClientState_e currentState_ {DxCraneScaleBLEClientState_Unknown};
    void setCurrentState(DxCraneScaleBLEClientState_e state);

    DxCraneScaleBLEClientValues_t lastValues_ {};
    CraneScaleBLEClientEventCallback eventCallback_ {nullptr};

    BLEScan *bleScanner_ = nullptr;
    BLEScan* getScanner();

    bool processData(const std::basic_string<char>& data);
    float convertManufacturerDataToWeight(const std::string &manufacturerDataStr);
    DxCraneScaleBLEClientHoldStatus_e readHoldStatus(const std::string &manufacturerDataStr);

    friend class DxCraneScaleBLEAdvertisedDeviceCallbacks;
};

class DxCraneScaleBLEAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
public:
    explicit DxCraneScaleBLEAdvertisedDeviceCallbacks() = default;

    //void onResult(BLEAdvertisedDevice advertisedDevice) override; // If not using NimBLE
    void onResult(BLEAdvertisedDevice *advertisedDevice) override; // If using NimBLE

private:
    DxCraneScaleBLEClient *client = nullptr;
    friend class DxCraneScaleBLEClient;
};

#endif //BIOFLOCFEEDER_DXCRANESCALEBLECLIENT_H
