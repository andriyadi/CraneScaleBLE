# CraneScaleBLE
So my team and I hacked a Crane Scale device with BLE to get the measurement data. 
This repo is an easy to use library to work such device.

Developed specifically for ESP32 with Arduino framework. It should be easy to change it to work with ESP-IDF.
I use **[NimBLE](https://github.com/h2zero/NimBLE-Arduino)**, instead of ESP32 Arduino's BLE library, for smaller firmware size.

Look at `examples/crane_tester.hpp` to get an idea how to use it.

## Note
To use the `DxCraneScaleDevice`, you should hack the hardware, by soldering some cables to various buttons and component, so we can use the class to control the crane scale, e.g:
* Turn the crane scale on/off
* Control the "hold" function
* Get active state by reading device's LCD backlight

Then connect the wires to ESP32 GPIOs. It looks something like this:
![Hack it!](https://raw.githubusercontent.com/andriyadi/CraneScaleBLE/main/assets/crane_scale_pcb.jpg)

## Credit
[@alwint3r](https://github.com/alwint3r) for hacking the protocol and helping to develop early client.