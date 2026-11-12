# Tech Context

## Target Hardware (DW2-WIFI V1.2, 2021.01.13)

- **MCU/Radio:** Opulinks OPL1000 A2 (`OPL1000 A2KA2205`, ARM Cortex-M3 + Wi-Fi + BLE).
- **Flash:** BoyaMicro 25Q80 (1 MB SPI flash).
- **Clocks:** 32.768 kHz (`X1`), 22 MHz (`X2`).
- **Interfaces:** `J2` UART (`3V3`, `GND`, `RXD`, `TXD`), test points (`RST`, `ADC`, `IO8`, `IO9`, `GND`), `H1` (3-pin Hall sensor, to confirm), `S1` button, `LED1`.
- **Power:** 2 × AAA (3 V). Never apply 5 V to `J2`; never connect `3V3` from USB-TTY while batteries are inserted.

## Software & Tools

- **SDK:** `https://github.com/Opulinks-Tech/OPL1000A2-SDK`
- **Reference Project:** `https://github.com/Opulinks-Tech/OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS`
- **Host OS:** Windows (`d:\_git\dw2-wifi-firmware`). Note: Python CLI scripts on this laptop may block on stdin unless `< NUL` is passed.
- **Hardware Tools Available:** 3.3 V USB-TTY adapter.
