# Progress

## Completed

- Initial device investigation, FCC ID lookup, PCB V1.2 component identification, and BLE pairing scan (`88:4A:18:EA:CD:A0`, `ck_1002873d3e`).
- Identified official Opulinks OPL1000 A2 SDK and reference CoolKit door sensor repo.
- Repository initialized with `.agent/plan.md`, `.gitignore`, `docs/investigation.md`, and `.agent/memory-bank/`.
- Cloned `OPL1000A2-SDK` and `OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS` into `third_party/`.
- Analyzed reference `blewifi` project and download tool guide:
  - Confirmed `ck_` prefix and `0xAAAA` BLE service UUID match stock DW2-Wi-Fi.
  - Reference GPIOs (`blewifi_configuration.h`): `BUTTON_IO_PORT=GPIO_IDX_04`, `MAGNETIC_IO_PORT=GPIO_IDX_05`, `BATTERY_IO_PORT=GPIO_IDX_02`, `LED_IO_PORT=GPIO_IDX_21`.
  - UARTs (`main_patch.c` + `hal_dbg_uart.c`): `DbgUart` (`IO0`/`IO1`) defaults to `115200` bps; `UART_IDX_1` (`IO8`/`IO9` or switched) uses `9600` bps (`BLEWIFI_SYS_UART_BAUDRATE`).
  - Download mode (`download_RELEASE.exe`): uses AT UART (`IO0`/`IO1`) at `115200` bps; click Download then press `RST` within 5 s. Tool does not support reading/dumping flash.
  - Build toolchain: `arm-none-eabi-gcc` 14.2.1 (`%USERPROFILE%\tools\bin`) + `GNU Make` 4.4.1 (`%USERPROFILE%\tools\make\bin`) installed; verified `ble_adv` builds cleanly (`opl1000_app_m3.bin`, 70,332 bytes) using a GCC `@args.rsp` response file (Windows `cmd.exe` 8191-char limit workaround).
- Step 5a (Custom BLE-only firmware skeleton) created in `firmware/` and compiled cleanly (`Output-GCC/dw2_ble.elf`: text=69941, data=632, bss=2652; `dw2_ble.bin`).
- Step 5b/c (Live validation via RAM download on `COM6`):
  - Downloaded into RAM via `download_RELEASE.exe` (`Download to RAM`).
  - BLE bursts received by PC scanner from `88:4A:18:EA:CD:A0`:
    - `0xFFFF: 4457 01 00 0200 7d0c` -> 'DW', v1, CLOSE (flags=0x00), counter=2, 3197 mV
    - `0xFFFF: 4457 01 01 0300 7d0c` -> 'DW', v1, OPEN (flags=0x01), counter=3, 3197 mV
    - `0xFFFF: 4457 01 00 0400 7d0c` -> 'DW', v1, CLOSE (flags=0x00), counter=4, 3197 mV
    - `0xFFFF: 4457 01 01 0500 7d0c` -> 'DW', v1, OPEN (flags=0x01), counter=5, 3197 mV
  - Zero packet loss across open/close cycle, monotonic counter working, battery reading ~3.2 V.

- Permanent flash write completed:
  - Verified cold boot directly from flash with 2xAAA batteries.
  - Initial boot broadcasted heartbeat packet `cnt=1`, subsequent magnet events `cnt=2..7` (CLOSE/OPEN transitions) with RSSI -52 to -72 dBm and 3197 mV battery.
- Created ESP32 BLE receiver component in `esp32_receiver/`:
  - `dw2_ble_receiver.hpp`: header-only NimBLE-Arduino callback handler with passive scanning (50% duty cycle, coexists seamlessly with ESP32 Wi-Fi).
  - `main.cpp`: example integration triggering heating cutoff on window open / resume on close.
  - `platformio.ini`: ready to build or copy into `smart-home-heating`.

## Pending

- None. Core goal achieved.

## Open Questions

1. Stock UART baud rate and boot logs.
2. Exact OPL1000 GPIO pins for `H1`, `S1`, `LED1`.
3. OPL1000 download/flash mode entry sequence.
4. Flash read/write protection status on stock unit.
5. Real sleep current with custom BLE firmware vs. stock ≤ 40 µA.
