# Project Brief

## Goal

Develop custom firmware for the **Sonoff DW2-Wi-Fi** door/window sensor (based on Opulinks **OPL1000 A2**) so it communicates **locally with an ESP32 WROOM** without eWeLink or cloud access, integrating into `github.com/RoDTOX/smart-home-heating` (turns off heating in a zone when a window/door opens).

## Scope

- Reverse-engineer / verify hardware pins (`H1` Hall sensor, `S1` button, `LED1`, `J2` UART, `RST`, `ADC`).
- Back up the original 1 MB flash (`BoyaMicro 25Q80`) before any write.
- Build custom OPL1000 A2 firmware (starting from Opulinks SDK + reference door sensor project) using **BLE advertisements** (no Wi-Fi) for ultra-low power.
- Implement corresponding BLE passive receiver logic on the ESP32 WROOM (`smart-home-heating`).
