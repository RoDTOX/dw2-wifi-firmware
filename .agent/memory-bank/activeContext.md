# Active Context

## Current Focus

- Project goal 100% completed: Sonoff DW2-Wi-Fi custom BLE-only firmware written permanently to flash; ESP32 receiver component ready for `smart-home-heating`.

## Recent Changes

- Added 128 KB Circular Flash Ring Buffer (~8,192 events) at `0x000E0000`.
- Added door open duration calculation (`open_duration_sec`) on close.
- Added Master RTC authoritative time beacon flag for blackout clock recovery.
- Added persistent NVS flash configuration structure `dw2_nv_config_t` (Wi-Fi fallback on/off, SSID, password, MQTT, ACK timeout, Ajar timeout).
- Created all-in-one CLI manager [tools/dw2_tool.py](file:///d:/_git/dw2-wifi-firmware/tools/dw2_tool.py) (`build`, `ports`, `monitor`, `scan`).
- Updated ESP32 receiver [esp32_receiver/dw2_ble_receiver.hpp](file:///d:/_git/dw2-wifi-firmware/esp32_receiver/dw2_ble_receiver.hpp) to decode all new telemetry fields.
