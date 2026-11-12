# Architectural Decisions

## ADR-001: BLE-Only Custom Firmware (Variant A)

- **Decision:** Use custom OPL1000 A2 firmware emitting BLE `Manufacturer Data` bursts on state changes and periodic heartbeats, with Wi-Fi disabled.
- **Rationale:** Minimizes power consumption (avoids 1–3 s Wi-Fi association), removes router/cloud dependency, and works directly with ESP32 WROOM BLE.
- **Mitigation:** Burst duration of 1–2 s + monotonic event counter handles packet loss when ESP32 shares radio between Wi-Fi and BLE.
