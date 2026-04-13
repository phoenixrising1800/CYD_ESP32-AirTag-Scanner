# CYD-ESP32-AirTag-Scanner

## Overview
This project allows for full Cheap Yellow Display (CYD) scanning AirTag MAC addresses and payloads without the need for an Android device or the nrfConnect app. ChatGPT wrote this: "This project is an advanced Bluetooth Low Energy (BLE) tracker detection system that runs on an ESP32, capable of identifying and logging the presence of Apple AirTags, Samsung SmartTags, and Tile Trackers. The system uses the ESP32’s BLE capabilities to scan for specific advertising patterns unique to these devices and displays the information on a connected TFT display." HAXXX

## The CYD has the following features:
- ESP32 (With Wifi and Bluetooth)
- 320 x 240 LCD Display (2.8")
- Touch Screen (Resistive)
- USB for powering and programming
- SD Card Slot, LED and some additional pins broken out

## Requirements
- 1 Cheap Yellow Display (If you need to buy one see https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display for links)
- "TFT_eSPI" library installed.
> Note: After install of the library, copy the file [User_Setup.h](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/DisplayConfig/User_Setup.h) to the `libraries\TFT_eSPI` Arduino folder. This sets up the library for use with this display.

## Instructions

### Flashing the Firmware
1. Connect your CYD-ESP32 to your computer
2. The CYD uses the CH340 USB to UART chip. If you do not have a driver already installed for this chip you may need to install one. Check out [Sparkfun's guide for installation instruction](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all)
3. You will need to have the ESP32 setup for your Arduino IDE, [instructions can be found here](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).
4. You can then select basically any ESP32 board in the boards menu. (I usually use "ESP32 Dev Module", but it doesn't really matter) If you see errors uploading a sketch, try setting board upload speed to `115200`
5. Wait for the flashing process to complete.

### Using the Scanner
1. Once flashing is complete, the device should reboot and start scanning for AirTags and display information such as MAC addresses and payloads.
