# System Patterns

## Chosen Architecture (Variant A: BLE-Only)

- **Sensor (OPL1000 A2 / ARM Cortex-M3):**
  - Sleeps in ultra-low-power mode; wakes on GPIO interrupt from Hall sensor `H1` (and periodic RTC timer).
  - Never enables Wi-Fi radio.
  - Emits BLE non-connectable advertisement burst (1–2 s) with custom `Manufacturer Data` payload: version, door state, monotonic event counter (persisted in flash), battery voltage (ADC), optional HMAC.
- **Receiver (ESP32 WROOM):**
  - Passive BLE scan (filtering by MAC / manufacturer ID) sharing radio with Wi-Fi; burst + monotonic counter handles packet loss and deduplication.
  - Missing heartbeat triggers "sensor offline" alert.

## Fallback Variants

- **Variant B:** Custom OPL1000 firmware using local Wi-Fi + UDP/MQTT.
- **Variant C:** Replace internals with ESP32-C3 + ESP-NOW (requires LiFePO4 3.2 V or boost converter for 2 × AAA).
