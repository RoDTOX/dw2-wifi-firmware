# Sonoff DW2-Wi-Fi Custom BLE Firmware & Local Integration

A complete, production-ready, open-source firmware replacement for the **Sonoff DW2-Wi-Fi** door/window sensor, built on the **Opulinks OPL1000 A2** SoC (ARM Cortex-M3 APS + Cortex-M0 MSQ).

Transforms a cloud-tethered, power-hungry Wi-Fi sensor into an **ultra-low-power, 100% local BLE smart sensor** featuring native **BTHome V2** integration, **Dual-Bank seamless Over-The-Air (OTA)** upgrades, an on-chip **128 KB SPI Flash blackbox flight recorder**, live **RTC time tracking**, and door ajar alarm warnings.

---

## 🌟 Key Features

- **100% Cloud-Free & Local:** Eliminates eWeLink, CoolKit servers, and Wi-Fi router dependency completely.
- **Ultra-Low Power Consumption:** Operates in `ps_smart_sleep` mode (~µA standby current). Power-hungry Wi-Fi radio is disabled. Years of battery life on standard AAA cells.
- **Home Assistant & BTHome V2 Ready:** Broadcasts open/closed events using standard BTHome V2 objects (`0x1A`), auto-discovered by Home Assistant via BLE Proxies or ESPHome.
- **Dual-Bank BLE OTA Upgrades:** True wireless firmware updates over BLE GATT service `0xAAAA`. Automatically alternates between Bank 0 (`0x00005000`) and Bank 1 (`0x0003E000`) with zero cables or disassembling needed after initial setup.
- **Door Open Duration Tracker:** Real-time tracking of open window/door duration, transmitted on close (`was_open_Xs`).
- **Door Ajar Alarm Warning:** Emits audible/visual alarm broadcasts every 15 seconds if a door is left open longer than 45 seconds.
- **128 KB SPI Flash Blackbox Recorder:** Dedicated circular ring buffer (`0x000E0000`–`0x000FFFFF`, 8,192 entries) storing timestamped historical open/close events with battery levels, retained across battery replacements.
- **Battery-Off Detection & Time Sync:** Automatic detection of battery removal/cold reboot via RTC sync status flags (`DW2_FLAG_TIME_SYNCED`), with one-click atomic clock sync over BLE.
- **Secret Knock & Button Trigger:** Open the 3-minute connectable maintenance/OTA window either by pressing the internal `S1` button or by performing 5 rapid magnet flicks (*Secret Knock*).
- **All-in-One CLI Manager (`dw2_tool.py`):** Cross-platform Python tool for compiling, serial flashing, real-time monitoring, BLE scanning, OTA flashing, RTC sync, and historical log dumping.

---

## 📐 Hardware Specifications (PCB V1.2)

| Component | Designator | Specification / Role |
|---|---|---|
| **SoC** | `U1` | Opulinks OPL1000 A2 (ARM Cortex-M3 @ 44–176 MHz + Cortex-M0) |
| **SPI Flash** | `U2` | BoyaMicro 25Q80 (1 MB / 8 Mbit SPI NOR Flash) |
| **Magnetic Sensor** | `H1` | Hall Effect switch on `IO5` (Active Low: Magnet present = Closed) |
| **Maintenance Button** | `S1` | Tactile push button on `IO4` (Active Low) |
| **Indicator LED** | `LED1` | Surface-mount LED on `IO21` (Active High) |
| **Battery AuxADC** | Internal | Resistor divider on `IO2` (millivolt battery monitoring) |
| **Programming Port** | `J2` | 4-pin header: `3V3`, `TXD`, `RXD`, `GND` |
| **Debug UART Output** | `IO8` | Single-wire 115200 bps Debug UART TX (bootloader & kernel logs) |
| **Power Source** | — | 2 × AAA Alkaline / NiMH batteries (nominal 3.0 V) |

### LED Feedback Signals

| Pattern | Duration / Cadence | Meaning |
|---|---|---|
| **3 Short Blinks** | 80 ms ON / 120 ms OFF | System cold boot / Hardware reset |
| **1 Blink** | 80 ms ON | Door Opened |
| **2 Blinks** | 80 ms ON / 120 ms OFF | Door Closed |
| **5 Rapid Blinks** | 50 ms ON / 70 ms OFF | Maintenance / BLE Connectable OTA Mode Activated (3 min) |

---

## 🗺️ SPI Flash Memory Map (1 MB)

| Address Range | Size | Allocation / Description |
|---|---|---|
| `0x00000000` – `0x00002FFF` | 12 KB | **2nd Boot Agent** (`opl1000_ota_loader.bin`) |
| `0x00003000` – `0x00003FFF` | 4 KB | **OTA Header 1** (Bank 0 descriptor, checksum, sequence number) |
| `0x00004000` – `0x00004FFF` | 4 KB | **OTA Header 2** (Bank 1 descriptor, checksum, sequence number) |
| `0x00005000` – `0x0003DFFF` | 228 KB | **Dual-Bank 0 (Primary Application Firmware)** |
| `0x0003E000` – `0x00076FFF` | 228 KB | **Dual-Bank 1 (OTA Staging Application Firmware)** |
| `0x00077000` – `0x000DFFFF` | 420 KB | Reserved / Opulinks RF calibration & NVS parameters |
| `0x000E0000` – `0x000FFFFF` | 128 KB | **DW2 Blackbox Event Logger** (32 sectors of 4 KB, 8,192 entries) |

---

## 🚀 Getting Started & First-Time Flashing

The initial transition from stock eWeLink firmware to custom firmware requires connecting a USB-to-UART adapter to write the **Dual-Bank Bootloader** once. Afterward, all future firmware updates are performed **100% wirelessly over BLE**.

### 1. Serial Pinout & Wiring

Solder four temporary jumper wires or use pogo-pins on the back of the DW2 PCB:

```text
  USB-to-UART Adapter             Sonoff DW2 PCB (J2 & Test Points)
  -------------------             ---------------------------------
  3.3V (VCC)          ----------> 3V3 (on J2 header)
  GND                 ----------> GND (on J2 header)
  RXD                 ----------> TXD (on J2 header)
  TXD                 ----------> RXD (on J2 header)
  
  [Optional / Debug]
  USB RX (2nd TTL)    ----------> IO8 test pad (Debug UART output @ 115200)
```

> [!CAUTION]
> Use **3.3V logic levels only**. 5V TTL will permanently damage the OPL1000 SoC. Remove the AAA batteries while powering via USB 3.3V.

### 2. Writing the Initial Dual-Bank Bootloader Image

1. Open `third_party/OPL1000A2-SDK/Tool/Download/download_RELEASE.exe`.
2. Select your USB COM port, Baud rate `115200`.
3. Choose the generated complete bootloader package:
   ```text
   firmware/Output-GCC/opl1000_ota.bin
   ```
4. Click **Start** in the download tool *before* powering the board.
5. Connect 3.3V power (or momentarily touch `RST` to `GND`). The ROM bootloader will catch the sync pattern and flash the full image in ~15 seconds.
6. Disconnect serial wires, re-insert AAA batteries, and close the case.

---

## 📡 Wireless OTA Upgrades Over BLE

Once the Dual-Bank bootloader is installed, you never need serial cables again.

### How to Trigger Maintenance Mode (3-minute connectable window):
- **Option A (Secret Knock):** Rapidly flick/tap the magnet against the sensor **5 times** within 2.5 seconds.
### Multi-Tier Hardware Button (S1) Actions:
- **Short Press (< 5s):** Enters 3-minute Connectable OTA & Maintenance mode (LED blinks 5 times).
- **Long Press (8–10s):** **Hard Reboot / SoC Reset** (LED blinks once at 8s cue, then 10 rapid blinks upon release followed by hardware reboot).
- **Very Long Press (≥ 15s):** **Factory Reset / Format Blackbox** (LED stays solid at 15s cue; upon release, erases all 128 KB SPI Flash log sectors, resets event counter to 0, and reboots).
- *Dead Zones (5–8s & 10–15s):* Release within these windows safely aborts without taking any destructive action.
- *Feedback:* The LED blinks **5 times rapidly** to confirm that the BLE GATT service (`0xAAAA`) is listening.

### Performing the Upgrade:

```powershell
# Build latest code and stream OTA firmware
python tools/dw2_tool.py build
python tools/dw2_tool.py ota
```

Output:
```text
=== [DW2 BLE OTA] Preparing upgrade ===
  Firmware: third_party/OPL1000A2-SDK/Tool/Download/Patch/opl1000.bin
  Size:     120,324 bytes (117.5 KB)
  Checksum: 0x00A2296E
  Target:   88:4A:18:EA:CD:A0

[OTA] Scanning for device 88:4A:18:EA:CD:A0...
[OTA] Found device DW2 (88:4A:18:EA:CD:A0). Connecting...
[OTA] Connected successfully!
[OTA] Device accepted upgrade! Streaming firmware chunks...
  [==============================] 100.0% (117.5/117.5 KB) @ 1.3 KB/s
[OTA] Synchronized RTC clock to 2026-11-13 01:52:50...
[SUCCESS] Firmware transfer & checksum verification completed!
[SUCCESS] Sensor successfully upgraded and rebooted into the new firmware!
```

---

## 🛠️ CLI Tool Reference (`tools/dw2_tool.py`)

The companion Python CLI provides complete operational control:

```powershell
# 1. Build firmware
python tools/dw2_tool.py build

# 2. Live BLE packet monitor & door event scanner
python tools/dw2_tool.py scan --timeout 30

# 3. Synchronize RTC internal clock
python tools/dw2_tool.py sync-time

# 4. Dump entire 128 KB historical blackbox log into a CSV file
python tools/dw2_tool.py dump-logs --out blackbox.csv

# 5. Over-the-air firmware flash
python tools/dw2_tool.py ota
```

---

## 📊 BLE Broadcast Packet Structure

Payload broadcast inside BLE `Manufacturer Specific Data` (`0xFF`):

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0` | Company ID | `uint16_t` | `0xFFFF` (Custom / Experimental) |
| `+2` | Magic Header | `uint8_t[2]` | `0xD2, 0x01` ('DW2') |
| `+4` | Protocol Version | `uint8_t` | `0x01` |
| `+5` | Status Flags | `uint8_t` | Bit 0: Open, Bit 1: Low Bat, Bit 2: Knock, Bit 3: Ajar Alarm, Bit 4: Time Synced |
| `+6` | Event Counter | `uint32_t` | Monotonically incrementing event sequence number |
| `+10` | Timestamp | `uint32_t` | Unix Epoch timestamp (0 if unsynced) |
| `+14` | Battery Voltage | `uint16_t` | Millivolts (e.g. `3197` = 3.197 V) |
| `+16` | Open Duration | `uint16_t` | Seconds door stayed open (valid on close) |
| `+18` | BTHome Object ID | `uint8_t` | `0x1A` (BTHome Door/Window sensor) |
| `+19` | BTHome Value | `uint8_t` | `0x00` = Closed, `0x01` = Open |

---

## 🔋 Battery Replacement & Power-Off Detection

When replacing AAA batteries:
1. The volatile RTC resets to `0` and flags `DW2_FLAG_TIME_SYNCED` as cleared.
2. In the BLE broadcast, `Time: Unsynced` notifies Home Assistant that a battery replacement occurred.
3. Simply trigger maintenance mode (5 flicks or S1) and run `sync-time` (or perform an OTA update) to re-align the real-time clock.
4. All existing historical logs in the **128 KB SPI Flash remain completely intact**.

---

## 📄 License & Credits

- **Firmware & CLI:** MIT License.
- **Opulinks OPL1000 SDK:** Copyright (c) Opulinks Technology Ltd.
