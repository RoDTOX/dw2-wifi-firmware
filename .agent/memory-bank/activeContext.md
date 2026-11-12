# Active Context

## Current Focus

- Project goal 100% completed: Sonoff DW2-Wi-Fi custom BLE-only firmware written permanently to flash; ESP32 receiver component ready for `smart-home-heating`.

## Recent Changes

- Created custom OPL1000 A2 BLE-only firmware in `firmware/` (`dw2_ble.bin`).
- Tested live in RAM and then permanently flashed to SPI flash via OPL1000 Download Tool.
- Created `esp32_receiver/dw2_ble_receiver.hpp`, `main.cpp`, and `platformio.ini`.
