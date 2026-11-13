# Architectural Decisions

## ADR-001: BLE-Only Custom Firmware (Variant A)

- **Decision:** Use custom OPL1000 A2 firmware emitting BLE `Manufacturer Data` bursts on state changes and periodic heartbeats, with Wi-Fi disabled.
- **Rationale:** Minimizes power consumption (avoids 1–3 s Wi-Fi association), removes router/cloud dependency, and works directly with ESP32 WROOM BLE.
- **Mitigation:** Burst duration of 500 ms + monotonic event counter handles packet loss when ESP32 shares radio between Wi-Fi and BLE.

## ADR-002: Dual-Bank BLE OTA with Custom 2nd Stage Loader

- **Decision:** Implement dual-bank OTA partitioned at Bank 0 (`0x00005000`) and Bank 1 (`0x0003E000`) managed by a 12 KB 2nd boot agent (`opl1000_ota_loader.bin`) and two 4 KB header descriptors (`0x00003000` and `0x00004000`).
- **Rationale:** Avoids bricking risk during wireless updates; if an OTA stream is interrupted or corrupted, the bootloader falls back to the previous intact bank.
- **Mitigation:** CRC32 and SHA256 image validation before bank switching.

## ADR-003: Clock Switching Before Software Reset

- **Decision:** Call `Hal_Sys_SwResetAll_patch()` instead of `Hal_Sys_SwResetAll()` on reboot.
- **Rationale:** OPL1000 M3 core hangs on warm software reset if the APS clock remains switched to PLL (`0b11`). The patch switches APS clock back to the 32 MHz crystal oscillator (`AOS_APS_CLK_SRC_XTAL`) prior to triggering `SYS_SW_RESET_ALL`.

## ADR-004: Timing Across Deep Sleep via FreeRTOS Tick RTC Compensation

- **Decision:** Use `xTaskGetTickCount()` instead of `osKernelSysTick()` for door open duration measurement.
- **Rationale:** ARM Cortex-M3 SysTick hardware timer halts when the CPU enters deep sleep (`ps_smart_sleep`). FreeRTOS compensated tick count tracks elapsed time accurately across sleep periods using the 32.768 kHz RTC.

## ADR-005: Standard CTS (0x1805) & Just Works Pairing (LE_SM_IO_CAP_NO_IO)

- **Decision:** Implement BLE Current Time Service (0x1805 / 0x2A2B) and configure SMP with `LE_SM_IO_CAP_NO_IO`.
- **Rationale:** Eliminates smartphone "Incorrect PIN" errors and enables standard OS time synchronization without proprietary app lock-in.

## ADR-006: Ergonomic Blackbox Log Dump Over BLE

- **Decision:** Default `dw2_tool.py dump-logs` to fetching the most recent 20 events with `--count` and `--all` options, and implement a 15-second BLE discovery scan window.
- **Rationale:** Downloading all 8,192 entries from the 128 KB SPI Flash circular ring buffer takes excessive time over BLE. Fast dump delivers immediate forensic feedback for routine checks.

## ADR-007: Dynamic Hardware MAC-Based BLE Local Name

- **Decision:** Form the BLE Complete Local Name dynamically as `DW2-%02X%02X` (e.g. `DW2-CDA0`) using the last two octets of the device MAC address read via `LeGapGetBdAddr()`.
- **Rationale:** Prevents collisions when multiple DW2 sensors are active and matches the physical identifier printed on the casing and product packaging.

## ADR-008: Boot Idempotence & Monotonic Counter Recovery (Claude Audit)

- **Decision:** Guard the cold boot sequence in `Dw2_CmMsgHandler` with `g_app.initialized` and resume `g_app.event_counter` from the latest valid SPI Flash log entry (`Dw2_FlashLog_GetLastCounter()`).
- **Rationale:** Discovered during Claude Code static audit; BLE advertising parameter re-configuration after disconnection re-triggered the boot branch, causing duplicate heartbeat timers and spurious LED blinks. Resuming the event counter ensures monotonic sequence continuity across battery swaps and warm boots.
