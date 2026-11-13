# Progress

## Completed

- Initial device investigation, FCC ID lookup, PCB V1.2 component identification, and BLE pairing scan (`88:4A:18:EA:CD:A0`, `ck_1002873d3e`).
- Cloned `OPL1000A2-SDK` and `OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS` into `third_party/`.
- Build toolchain configured: `arm-none-eabi-gcc` 14.2.1 and `GNU Make` 4.4.1.
- Custom BLE-only firmware designed with BTHome V2 object broadcast (`0x1A`), ultra-low power `ps_smart_sleep`, and 45s Door Ajar alert.
- Dual-Bank BLE OTA System built and verified:
  - Custom 2nd stage boot agent `opl1000_ota_loader.bin` (12 KB at `0x00000000`).
  - Dual bank descriptors (Header 1 at `0x00003000`, Header 2 at `0x00004000`).
  - Active Bank 0 (`0x00005000`) and Bank 1 (`0x0003E000`).
  - Live wireless flashing via Opulinks OTA GATT service (`0xAAAA`).
- Clock & Reset Glitch Resolved:
  - Invoking `Hal_Sys_SwResetAll_patch()` to switch APS clock from PLL back to XTAL before software reset, enabling warm reset without hanging.
- Timing & Door Open Duration Accuracy:
  - Switched from frozen `osKernelSysTick()` to FreeRTOS `xTaskGetTickCount()` (compensated across sleep intervals by 32.768 kHz RTC). Verified exact timing (`was_open_3s`, `was_open_11s`).
- BLE Security & Time Synchronization:
  - Added BLE Security Manager (`LE_SM_IO_CAP_NO_IO`, Just Works mode) to eliminate PIN errors on modern smartphones.
  - Added standard Current Time Service (CTS 0x1805 / 0x2A2B) to allow automatic time sync from phones or CLI.
- Maintenance & Fast Dump:
  - Secret 5-flick magnetic knock and S1 button open a 3-minute connectable window.
  - Immediate `LeGapAdvertisingEnable(TRUE)` added upon S1/knock trigger.
  - CLI `dw2_tool.py dump-logs` now scans up to 15s and defaults to fast retrieval of the last 20 entries (with `--count` and `--all` options).
- ESP32 receiver component in `esp32_receiver/` ready for smart-home integration.
- Public documentation in `README.md`.

## Pending

- None. All functional and ergonomic goals achieved.
