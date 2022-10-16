//
// Created by Andri Yadi on 22/09/22.
// Tester code. Using M5 Core/Tough ESP32
//

#ifndef EXPLORE_CRANE_TESTER_HPP
#define EXPLORE_CRANE_TESTER_HPP

#include "Arduino.h"
#include "M5Unified.h"
#include "DxCraneScaleDevice.h"

#define UI_WITH_BUTTON 1

DxCraneScaleDevice scaleDevice = DxCraneScaleDevice(13, 14);

//#include "CraneScaleBLEClient.h"
//CraneScaleBLEIdentifier bleClientId;
//CraneScaleBLEClient bleClient = CraneScaleBLEClient(bleClientId);

#if UI_WITH_BUTTON
bool isInMeasurementMode = false;

LGFX_Button button;
static int32_t w;
static int32_t h;
#endif

void setup() {
    Serial.begin(115200);

    auto cfg = M5.config();
    M5.begin(cfg);

#if UI_WITH_BUTTON
    M5.Lcd.setFont(&fonts::Font2);
    w = M5.Lcd.width();
    h = M5.Lcd.height();
    button.initButton(&M5.Lcd, w / 2 , h / 2, 100, 50, TFT_RED, TFT_YELLOW, TFT_BLACK, "Start" );
    button.drawButton();

    M5.Lcd.setFont(&fonts::Font4);
    //M5.Lcd.setCursor(5, h / 2 + 60);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setTextDatum(top_center);
    //M5.Lcd.print("== DATA ==");
    M5.Lcd.drawString("== DATA ==", w / 2, h / 2 + 60);

#endif

    scaleDevice.begin();

    // Wait indefinitely to be connected to crane scale device
    if (scaleDevice.waitForConnected()) {
        Serial.println("SCALE DEVICE IS CONNNECTED!");
    }
}

float lastReadWeight = 0.0f;
void loop() {
    M5.update();
    //    bleClient.run();

#if UI_WITH_BUTTON

    std::int32_t tx, ty;
    auto tc = M5.Display.getTouch(&tx, &ty);
    if (tc > 0) {
        if (button.contains(tx, ty)) {
            button.press(true);
        }
    }
    else {
        button.press(false);
    }

    if (button.justPressed()) {
        M5.Lcd.setFont(&fonts::Font2);
#if CRANESCALE_RESPECT_MEASUREMENT_MODE
        if (!isInMeasurementMode) {
            button.drawButton(false, "Waiting");
            bool _res = scaleDevice.startMeasurement(true);
            if (_res) {
                button.drawButton(true, "Stop");
                isInMeasurementMode = true;
            }
        }
        else {
            button.drawButton(false, "Start");
            if (scaleDevice.stopMeasurement()) {
                isInMeasurementMode = false;
            }
        }
#endif
    }
#endif

    scaleDevice.run();

    if (lastReadWeight != scaleDevice.getLastValues().weight) {
        M5.Lcd.setFont(&fonts::Font4);
        M5.Lcd.fillRect(0, h / 2 + 58, w, 70, TFT_BLACK);
        String _weightStr = String(scaleDevice.getLastValues().weight, 2) + " kg";
        M5.Lcd.drawString(_weightStr, w / 2, h / 2 + 60);
        lastReadWeight = scaleDevice.getLastValues().weight;
    }
}

#endif //EXPLORE_CRANE_TESTER_HPP
