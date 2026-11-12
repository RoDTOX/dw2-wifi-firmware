# Product Context

## Why This Exists

- Stock Sonoff DW2-Wi-Fi firmware uses BLE only for eWeLink pairing and sends door events exclusively over Wi-Fi to the CoolKit/eWeLink cloud.
- Local smart-heating control requires reliable, low-latency, cloud-free open/closed notifications and long battery life on 2 × AAA cells.

## Target User Behavior

- Door/window opens or closes → DW2 wakes from deep/smart sleep via Hall sensor (`H1`), broadcasts a 1–2 s burst of BLE `Manufacturer Data` advertisements (version, state, event counter, battery voltage, optional HMAC), and returns to sleep (~µA).
- Periodic heartbeat advertisement every few hours reports battery level and confirms sensor liveness.
- ESP32 WROOM continuously scans BLE passively, deduplicates via event counter, and triggers zone heating cutoff.
