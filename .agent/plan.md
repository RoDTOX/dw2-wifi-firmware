# Plan: DW2-Wi-Fi Custom Firmware & Local ESP32 Integration

## Original request (verbatim)

```text
1-2,am si usb tty daca e nevoie.
```

Context din mesajul inițial: inițializare repo (`docs/`, `.agent/memory-bank/`), clonare și analiză `OPL1000A2-SDK` + `OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS` pentru a răspunde la întrebările deschise (mod boot/flash, baud rate UART, pini de referință, toolchain GCC), cu pregătire pentru conectarea adaptorului USB-TTY 3.3 V la `J2`.

## Stage tracker

- [x] Creare `.agent/plan.md` și `.gitignore`
- [x] Salvare rezumat investigație în `docs/investigation.md`
- [x] Inițializare `.agent/memory-bank/` (7 fișiere de bază)
- [x] Clonare `OPL1000A2-SDK` și `OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS` în `third_party/`
- [x] Analiză SDK & proiect de referință (baud rate UART, mod download/boot, pini H1/S1/LED1, toolchain GCC)
- [x] Pasul 1: Citire UART pe `J2` / `IO8` cu adaptorul USB-TTY 3.3 V (`IO8` = Debug UART @ 115200 bps, confirmat firmware bazat pe `blewifi`, `LOW`=CLOSE / `HIGH`=OPEN)
- [ ] Pasul 2: Backup flash 1 MB (`25Q80`) *(amânat / omis cu acordul utilizatorului)*
- [x] Pasul 4a: Instalare `arm-none-eabi-gcc` (14.2.1) + `GNU Make` (4.4.1) și compilare de test SDK (`ble_adv` → `opl1000_app_m3.bin`, 70332 B via `@args.rsp`)
- [x] Pasul 5a: Schelet firmware BLE-only (`firmware/src/dw2_app.c`, `dw2_protocol.h`, `dw2_config.h`, `main_patch.c`, `Makefile`) compilat curat → `dw2_ble.bin` (70.5 KB)
- [x] Pasul 5b: Încărcare în RAM prin `J2` (`COM6`) cu `download_RELEASE.exe`
- [x] Pasul 5c: Validare live BLE (`88:4A:18:EA:CD:A0`, `0xFFFF` "DW", evenimente `0x00`=CLOSE / `0x01`=OPEN, monotonic counter 2->3->4->5, 3197 mV)
- [x] Pasul 6: Scrierea permanentă în flash a firmware-ului `dw2_ble` (validat cold boot, contor 1..7)
- [x] Pasul 7: Componentă receptor BLE pe ESP32 WROOM (`esp32_receiver/dw2_ble_receiver.hpp`, `main.cpp`, `platformio.ini`) gata pentru `smart-home-heating`

## Verification steps

- Verificare existență și structură `.agent/memory-bank/` și `docs/investigation.md`.
- Verificare că `third_party/` este ignorat în `.gitignore`.
- Citire directă a fișierelor din SDK / proiect de referință pentru toate datele raportate (baud rate, pini, mod boot, toolchain).
