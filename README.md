# CraneScaleBLE
So my team and I hacked a Crane Scale device with BLE to get the measurement data. 
This repo is an easy to use library to work such device.

Developed specifically for ESP32 with Arduino framework. It should be easy to change it to work with ESP-IDF.
I use **[NimBLE](https://github.com/h2zero/NimBLE-Arduino)**, instead of ESP32 Arduino's BLE library, for smaller firmware size.

Look at `examples/crane_tester.hpp` to get an idea how to use it.

