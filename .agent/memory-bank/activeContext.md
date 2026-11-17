# Active Context

## Current Focus

- Sonoff DW2-Wi-Fi liberated into a 100% local, autonomous BLE smart sensor with Dual-Bank BLE OTA, BTHome V2 broadcast, 128 KB flight recorder, CTS real-time sync, and fast log dump CLI.
- Sensor is fully operational, verified in Home Assistant with instantaneous BTHome V2 telemetry and stable door open/close events.
- Added zero-install Web Companion Dashboard (`web/index.html` on `http://localhost:8080`) supporting live telemetry, clock synchronization, blackbox log viewing/export, and ready-to-import Home Assistant blueprints.

## Recent Changes

- **Automatic USB-UART Serial Port Detection**: Upgraded `tools/auto_flasher.py`, `tools/opl1000_flasher.py`, and `tools/dw2_tool.py` with cross-platform auto-detection of USB-to-UART bridge adapters (CH340, CP210x, FTDI), eliminating hardcoded `COM7`.
- **Blackbox Log Retrieval Verified**: Successfully dumped 100 historical events over BLE (indices #5814 to #5913 out of 5,914 total entries in Flash) and verified epoch timestamps.
- **Web Bluetooth Windows WinRT Hardening**: Fixed GATT disconnection deadlocks on Windows by setting BLE SMP `bond = FALSE`, implementing a 600ms WinRT stabilization delay, and adding a 3-attempt connection retry loop in `web/index.html`.
- **RTC Clock Sync & Unsynced Warning**: Added visual micro-blinks if the clock is uncalibrated, 400ms confirmation glow on sync, and SPI Flash restoration of last known epoch on boot.
- **Home Assistant Verified**: Home Assistant automatically receives BTHome V2 packets (`0xFCD2`) on door state changes without requiring cloud or custom integrations.
