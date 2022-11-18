//
// Created by Andri Yadi on 22/09/22.
//

#include "DxCraneScaleBLEClient.h"

DxCraneScaleBLEIdentifier::~DxCraneScaleBLEIdentifier() {
    delete fallbackDeviceAddress_;
}

DxCraneScaleBLEIdentifier::DxCraneScaleBLEIdentifier(const std::string &fallbackAddr) {
    if (!fallbackAddr.empty()) {
        fallbackDeviceAddress_ = new BLEAddress(fallbackAddr);
    }
}

// If using NimBLE
#ifdef MAIN_NIMBLEDEVICE_H_
void DxCraneScaleBLEAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice *device) {
    if (client == nullptr) {
        return;
    }

    if (!device->getAddress().equals(*client->getAddress())) {
        //log_e("Address not match -> %s", device.getAddress().toString().c_str());
        //client->setCurrentState(CraneScaleBLEClientState_Began);
        return;
    }

    int rssi = device->getRSSI();
    client->lastRSSI_ = rssi;

    log_d("RSSI: %d", rssi);

    if (rssi < client->minAcceptedRSSI_)
    {
        return;
    }

    if (!device->haveManufacturerData()) {
        log_e("Not receiving necessary data");
        return;
    }

    if (client->currentState_ == DxCraneScaleBLEClientState_Scanning) {
        client->setCurrentState(DxCraneScaleBLEClientState_Found);
    }

    client->processData(device->getManufacturerData());
}
#else // If not using NimBLE
void CraneScaleBLEAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice device) {
    if (client == nullptr) {
        return;
    }

    if (!device.getAddress().equals(*client->getAddress())) {
        //log_e("Address not match -> %s", device.getAddress().toString().c_str());
        //client->setCurrentState(CraneScaleBLEClientState_Scanning);
        return;
    }

    if (!device.haveManufacturerData()) {
        log_e("Not receiving necessary data");
        return;
    }

    if (client->currentState_ == CraneScaleBLEClientState_Scanning) {
        client->setCurrentState(CraneScaleBLEClientState_Found);
    }

    client->processData(device.getManufacturerData());
}
#endif


DxCraneScaleBLEClient::DxCraneScaleBLEClient(DxCraneScaleBLEIdentifier &id): identifiers_(id) {

}

DxCraneScaleBLEClient::~DxCraneScaleBLEClient() = default;

bool DxCraneScaleBLEClient::begin() {

    log_i("Starting Crane Scale BLE Client...");

#if CRANESCALE_BLECLIENT_SCAN_FOREVER
    BLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE);//CONFIG_BTDM_SCAN_DUPL_TYPE_DATA);
    NimBLEDevice::setScanDuplicateCacheSize(20);
#endif

    BLEDevice::init("");

    // Lower the scan power as the device should be near
    //BLEDevice::setPower(ESP_PWR_LVL_N3, ESP_BLE_PWR_TYPE_SCAN);

    BLEDevice::setPower(ESP_PWR_LVL_P3, ESP_BLE_PWR_TYPE_SCAN);

    log_d(" - Device initialized");

    setCurrentState(DxCraneScaleBLEClientState_Began);

#if CRANESCALE_BLECLIENT_SCAN_FOREVER
    scanForever();
#else
    scan();
#endif

    return true;
}

bool DxCraneScaleBLEClient::run() {

#if CRANESCALE_BLECLIENT_SCAN_FOREVER
    scanForever(); // Will start scan if not yet, else nothing to do.
#else
    // If never found
    if (currentState_ == DxCraneScaleBLEClientState_Scanning) {
        if (millis() - timeTracker_.lastScan > 10000) {
            scan();
        }
    }

    // If previously already found and more, keep scanning to get measurement data
    if (currentState_ >= DxCraneScaleBLEClientState_Found) {
        //if (millis() - timeTracker_.lastScan > 500) {
        if (millis() - timeTracker_.lastScan > 300) {
            scan(); // This is to keep reading measurement data
        }

        // If last received data is too long, assumed device is missing or turned off
        if (millis() - timeTracker_.lastReceivedData > 5000) {
            setCurrentState(DxCraneScaleBLEClientState_Scanning);
        }
    }
#endif

    return true;
}

void DxCraneScaleBLEClient::setCurrentState(DxCraneScaleBLEClientState_e state) {
    currentState_ = state;
    if (currentState_ == DxCraneScaleBLEClientState_Began) {
        log_i("State: BEGAN");
    }
    else if (currentState_ == DxCraneScaleBLEClientState_Scanning) {
        log_i("State: SCANNING");
    }
    else if (currentState_ == DxCraneScaleBLEClientState_Found) {
        log_i("State: FOUND");
    }
    else if (currentState_ == DxCraneScaleBLEClientState_Measuring) {
        log_i("State: MEASURING..");
    }
    else {
        log_i("Current state: %d", currentState_);
    }

    if (eventCallback_) {
        eventCallback_(lastValues_, currentState_);
    }
}

BLEScan *DxCraneScaleBLEClient::getScanner() {
    if (bleScanner_ == nullptr) {
        bleScanner_ = BLEDevice::getScan();

        auto *_advCB = new DxCraneScaleBLEAdvertisedDeviceCallbacks();
        _advCB->client = this;

        bleScanner_->setAdvertisedDeviceCallbacks(_advCB, false);
        bleScanner_->setActiveScan(true);

        bleScanner_->setInterval(100);
        bleScanner_->setWindow(99);

//        bleScanner_->setInterval(97); // How often the scan occurs / switches channels; in milliseconds,
//        bleScanner_->setWindow(37);  // How long to scan during the interval; in milliseconds.
//        bleScanner_->setMaxResults(0); // do not store the scan results, use callback only.

        setCurrentState(DxCraneScaleBLEClientState_Scanning);
    }

    return bleScanner_;
}

BLEScanResults DxCraneScaleBLEClient::scan() {
    //log_i("Scanning requested device...");

    auto _res = getScanner()->start(1, false);
    timeTracker_.lastScan = millis();
    //setCurrentState(CraneScaleBLEClientState_Scanning);

    return _res;
}

bool DxCraneScaleBLEClient::scanForever() {

    if (!getScanner()->isScanning()) {
        // If an error occurs that stops the scan, it will be restarted here.
        log_i("Scanning is restarted!");
        auto _res = getScanner()->start(0, nullptr, false);
        timeTracker_.lastScan = millis();
        setCurrentState(DxCraneScaleBLEClientState_Scanning);

        return _res;
    }

    return true;
}

bool DxCraneScaleBLEClient::stopScan() {
    setCurrentState(DxCraneScaleBLEClientState_Began);
    return getScanner()->stop();
}

bool DxCraneScaleBLEClient::processData(const std::basic_string<char>& strManufacturerData) {

    if (strManufacturerData.empty()) {
        return false;
    }

//    Serial.print("Manufacturer data: ");
//    for (char i : strManufacturerData)
//    {
//        Serial.printf("%d ", i);
//    }
//    Serial.println();

    timeTracker_.lastReceivedData = millis();
    auto _w = convertManufacturerDataToWeight(strManufacturerData);
    auto _status = readHoldStatus(strManufacturerData);

    if (lastValues_.weight != _w || lastValues_.holdStatus != _status) {
        lastValues_.weight = _w;
        lastValues_.holdStatus = _status;
        log_i("Weight: %.4f (%s)", lastValues_.weight, (lastValues_.holdStatus == DxCraneScaleBLEClientHoldStatus_Hold) ? "H" : "M");
    }

    if (currentState_ != DxCraneScaleBLEClientState_Measuring && lastValues_.holdStatus == DxCraneScaleBLEClientHoldStatus_Measuring) {
        setCurrentState(DxCraneScaleBLEClientState_Measuring);
    }
    else if (currentState_ != DxCraneScaleBLEClientState_MeasurementHold && lastValues_.holdStatus == DxCraneScaleBLEClientHoldStatus_Hold) {
        setCurrentState(DxCraneScaleBLEClientState_MeasurementHold);
    }
    //else {

    if (eventCallback_) {
        eventCallback_(lastValues_, currentState_);
    }

    //}

    return true;
}

float DxCraneScaleBLEClient::convertManufacturerDataToWeight(const std::string &manufacturerDataStr) {
    if (manufacturerDataStr.length() < 19) {
        return NAN;
    }

    auto manufacturerDataBuf = manufacturerDataStr.data();
    uint16_t raw = manufacturerDataBuf[12] << 8 | manufacturerDataBuf[13];

    return ((float)raw / 100.0f);
}

DxCraneScaleBLEClientHoldStatus_e DxCraneScaleBLEClient::readHoldStatus(const std::string &manufacturerDataStr) {
    if (manufacturerDataStr.length() < 19) {
        return DxCraneScaleBLEClientHoldStatus_Unknown;
    }

    auto status = manufacturerDataStr.at(16);
    if ((uint8_t)status == 1) {
        return DxCraneScaleBLEClientHoldStatus_Measuring;
    }
    else if ((uint8_t)status == 0xA1) {
        return DxCraneScaleBLEClientHoldStatus_Hold;
    }
    else {
        return DxCraneScaleBLEClientHoldStatus_Unknown;
    }
}




