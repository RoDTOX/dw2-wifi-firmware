# Active Context

## Current Focus

- Sonoff DW2-Wi-Fi liberated into a 100% local, autonomous BLE smart sensor with Dual-Bank BLE OTA, BTHome V2 broadcast, 128 KB flight recorder, CTS real-time sync, and fast log dump CLI.

## Recent Changes

- **Dual-Bank BLE OTA**: Full custom 2nd stage bootloader (`opl1000_ota_loader.bin` at `0x00000000`), Bank 0 (`0x00005000`) and Bank 1 (`0x00003E000`), and live wireless flashing verified via GATT `0xAAAA`.
- **Clock & Software Reset Fix**: Switched to `Hal_Sys_SwResetAll_patch()` to ensure APS clock drops back from PLL to XTAL before reboot, eliminating post-OTA hang.
- **Door Open Duration Tracking**: Replaced `osKernelSysTick()` with FreeRTOS `xTaskGetTickCount()` (compensated across `ps_smart_sleep` intervals by 32.768 kHz RTC).
- **CTS Real-Time Sync**: Added standard BLE Current Time Service (0x1805 / 0x2A2B) and epoch conversion; mobile phones and CLI can set authoritative sensor time.
- **BLE SMP Just Works**: Configured `LE_SM_IO_CAP_NO_IO` to eliminate "Incorrect PIN" popups on smartphones.
- **Immediate Connectable Advertising**: Fixed `Dw2_EnterConnectableMode` to explicitly start advertising (`LeGapAdvertisingEnable(TRUE)`) immediately upon S1 press or 5 magnet flicks.
- **CLI Ergonomics**: Updated `dw2_tool.py dump-logs` to scan for device with a 15-second discovery window and default to fetching the last 20 entries with `--count` and `--all` options.
