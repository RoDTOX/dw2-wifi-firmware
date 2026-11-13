# Sonoff DW2-Wi-Fi Custom BLE Firmware & Local Integration

A complete, production-ready, open-source firmware replacement for the **Sonoff DW2-Wi-Fi** door/window sensor, built on the **Opulinks OPL1000 A2** SoC (ARM Cortex-M3 APS + Cortex-M0 MSQ).

Transforms a cloud-tethered, power-hungry Wi-Fi sensor into an **ultra-low-power, 100% local BLE smart sensor** featuring native **BTHome V2** integration, **Dual-Bank seamless Over-The-Air (OTA)** upgrades, an on-chip **128 KB SPI Flash blackbox flight recorder**, live **RTC time tracking**, door ajar alarm warnings, and a multi-tier hardware button (Reboot & Factory Reset).

---

## 🌟 Key Features

- **100% Cloud-Free & Local:** Eliminates eWeLink, CoolKit servers, and Wi-Fi router dependency completely.
- **Ultra-Low Power Consumption:** Operates in `ps_smart_sleep` mode (~30 µA standby current). Power-hungry Wi-Fi radio is completely disabled. ~2 years of battery life on standard AAA alkaline cells.
- **Home Assistant & BTHome V2 Ready:** Broadcasts open/closed events using standard BTHome V2 objects (`0x1A`), auto-discovered by Home Assistant via Bluetooth Proxies or ESPHome without custom integrations.
- **Unique Hardware-Derived Bluetooth Name:** Advertises as `DW2-XXXX` (e.g. `DW2-CDA0`), where `XXXX` matches the last two bytes of the MAC address printed on the physical sensor casing.
- **Dual-Bank BLE OTA Upgrades:** True wireless firmware updates over BLE GATT service `0xAAAA`. Automatically alternates between Bank 0 (`0x00005000`) and Bank 1 (`0x0003E000`) with zero cables or disassembling needed after initial setup.
- **Door Open Duration Tracker:** Real-time tracking of open window/door duration, transmitted on close (`was_open_Xs`).
- **Door Ajar Alarm Warning:** Emits audible/visual alarm broadcasts every 15 seconds if a door is left open longer than 45 seconds.
- **128 KB SPI Flash Blackbox Recorder:** Dedicated circular ring buffer (`0x000E0000`–`0x000FFFFF`, 8,192 entries) storing timestamped historical open/close events with battery levels, retained across battery replacements.
- **Multi-Tier Hardware Button (S1):** Short press enters 3-minute OTA mode, 8–10s hold triggers SoC hard reboot, and ≥15s hold executes a clean factory format of the blackbox flash.
- **Secret Knock Gesture:** Open the 3-minute connectable maintenance/OTA window by performing 5 rapid magnet flicks (*Secret Knock*).
- **All-in-One CLI Manager (`dw2_tool.py`):** Cross-platform Python tool for compiling, serial flashing, real-time monitoring, BLE scanning, OTA flashing, RTC sync, and historical log dumping with binary search.
- **Ready-to-Use ESP32 Receiver:** Header-only C++ driver in `esp32_receiver/` ready to plug into heating controllers or smart-home gateways.

---

## 📐 Hardware Specifications (PCB V1.2)

| Component | Designator | Specification / Role |
|---|---|---|
| **SoC** | `U1` | Opulinks OPL1000 A2 (ARM Cortex-M3 @ 44–176 MHz + Cortex-M0) |
| **SPI Flash** | `U2` | BoyaMicro 25Q80 (1 MB / 8 Mbit SPI NOR Flash) |
| **Magnetic Sensor** | `H1` | Hall Effect switch on `IO5` (Active Low: Magnet present = Closed) |
| **Maintenance Button** | `S1` | Tactile push button on `IO4` (Active Low) |
| **Indicator LED** | `LED1` | Surface-mount LED on `IO21` (Active High) |
| **Battery AuxADC** | Internal | Opulinks internal supply rail measurement with factory OTP calibration |
| **Programming Port** | `J2` | 4-pin header: `3V3`, `TXD`, `RXD`, `GND` |
| **Debug UART Output** | `IO8` | Single-wire 115200 bps Debug UART TX (bootloader & kernel logs) |
| **Power Source** | — | 2 × AAA Alkaline / NiMH batteries (nominal 3.0 V) |

---

## 🎛️ LED Feedback & Button Gestures

### LED Indications

| Pattern | Duration / Cadence | Meaning |
|---|---|---|
| **3 Short Blinks** | 80 ms ON / 120 ms OFF | System cold boot / Hardware reset |
| **1 Blink** | 80 ms ON | Door Opened |
| **2 Blinks** | 80 ms ON / 120 ms OFF | Door Closed |
| **5 Rapid Blinks** | 50 ms ON / 70 ms OFF | Maintenance / BLE Connectable OTA Mode Activated (3 min) |
| **1 Single Blink (at 8s)** | 100 ms ON | Cue: "Release now to Hard Reboot" |
| **10 Rapid Blinks** | 40 ms ON / 40 ms OFF | Reboot sequence initiated |
| **Solid LED (at 15s)** | Continuous ON | Cue: "Release now to Factory Format Blackbox" |

### Multi-Tier S1 Button & Magnet Actions

| Action | Hold Duration / Gesture | LED Feedback | Result |
|---|---|---|---|
| **Maintenance / OTA** | Press `< 5s` or **5 rapid magnet flicks** | 5 rapid blinks | Opens 3-minute BLE GATT connectable window (`0xAAAA`). |
| **Abort / Cancel** | Release between `5s` – `8s` | None | Safe cancellation window — no state change. |
| **Hard Reboot** | Hold `8s` – `10s` | 1 blink at 8s, then 10 rapid blinks | Restarts SoC via `Hal_Sys_SwResetAll_patch()`. Keeps all Flash logs intact. |
| **Abort / Cancel** | Release between `10s` – `15s` | None | Safe cancellation window. |
| **Factory Reset** | Hold `≥ 15s` | LED turns solid at 15s | Erases entire 128 KB Flash log (`0x000E0000`–`0x000FFFFF`), resets event counter to 0, and reboots. |

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

### Step 1: Wake Sensor into Maintenance Mode
- Press the `S1` button once (or flick the magnet 5 times). The LED blinks 5 times rapidly.

### Step 2: Stream Firmware via CLI
```powershell
# Compile firmware and stream wirelessly over BLE
python tools/dw2_tool.py build
python tools/dw2_tool.py ota
```

Output:
```text
=== [DW2 BLE OTA] Preparing upgrade ===
  Firmware: third_party/OPL1000A2-SDK/Tool/Download/Patch/opl1000.bin
  Size:     122,324 bytes (119.5 KB)
  Checksum: 0x00A53D19
  Target:   88:4A:18:EA:CD:A0

[OTA] Scanning for device 88:4A:18:EA:CD:A0 (up to 35s)...
[OTA] Found device DW2-CDA0 (88:4A:18:EA:CD:A0). Connecting...
[OTA] Connected successfully!
[OTA] Device accepted upgrade! Streaming firmware chunks...
  [==============================] 100.0% (119.5/119.5 KB) @ 1.1 KB/s
[OTA] All chunks transferred! Finalizing and verifying checksum...
[SUCCESS] Sensor verified firmware and triggered hardware reboot!
```

---

## 🛠️ CLI Tool Reference (`tools/dw2_tool.py`)

The companion Python CLI provides complete operational control:

```powershell
# 1. Compile firmware binary
python tools/dw2_tool.py build

# 2. Live BLE packet monitor & door event scanner
python tools/dw2_tool.py scan --timeout 30

# 3. Synchronize RTC internal clock from host PC
python tools/dw2_tool.py sync-time

# 4. Dump the last 20 historical blackbox entries (default, takes ~0.5s via binary search)
python tools/dw2_tool.py dump-logs

# 5. Dump specific count or export full 128 KB log to CSV
python tools/dw2_tool.py dump-logs -n 50 --out history.csv
python tools/dw2_tool.py dump-logs --all --out full_blackbox.csv

# 6. Wireless firmware OTA flash
python tools/dw2_tool.py ota
```

---

## 📊 BLE Broadcast Packet Structure

Every door movement and periodic heartbeat broadcasts a 20-byte payload inside BLE `Manufacturer Specific Data` (`0xFF`):

| Offset | Field | Type | Description |
|---|---|---|---|
| `+0` | Company ID | `uint16_t` | `0xFFFF` (Custom / Experimental) |
| `+2` | Magic Header | `uint8_t[2]` | `'D'`, `'W'` (`0x44, 0x57`) |
| `+4` | Protocol Version | `uint8_t` | `0x02` (Epoch timestamp + BTHome V2) |
| `+5` | Status Flags | `uint8_t` | Bitfield (see below) |
| `+6` | Event Counter | `uint32_t` | Monotonic sequential counter (anti-replay) |
| `+10` | Epoch Timestamp | `uint32_t` | Unix Epoch UTC seconds (`0` if unsynced) |
| `+14` | Battery Voltage | `uint16_t` | Millivolts (e.g. `3050` = 3.05 V) |
| `+16` | Open Duration | `uint16_t` | Duration door was open in seconds (sent on Close) |
| `+18` | BTHome Object ID | `uint8_t` | `0x1A` (Standard BTHome V2 Door Object) |
| `+19` | BTHome Value | `uint8_t` | `0x00` = Closed, `0x01` = Open |

### Status Flags (`uint8_t` Bitfield)

- `0x01` (**DOOR_OPEN**): `1` = Door Open, `0` = Door Closed.
- `0x02` (**HEARTBEAT**): `1` = Periodic keepalive ping (every 4 hours).
- `0x04` (**BUTTON_TEST**): `1` = Event triggered via S1 button press.
- `0x08` (**DOOR_AJAR**): `1` = Warning! Door has remained open for `> 45s` (repeats every 15s).
- `0x10` (**TIME_SYNCED**): `1` = RTC clock is synchronized to valid real-world time.
- `0x20` (**LOW_BATTERY**): `1` = Battery voltage dropped below 2.3 V.

---

## 🏠 Home Assistant Integration

Because the firmware includes native **BTHome V2** data (`0x1A`), Home Assistant automatically discovers the sensor as soon as it transmits:

1. Ensure Bluetooth or a Bluetooth Proxy (e.g., ESPHome BLE Proxy) is running on your network.
2. Under **Settings -> Devices & Services**, Home Assistant will show a notification: **"New device discovered: BTHome"**.
3. Click **Configure** — no passwords, PINs, or cloud accounts required.
4. The entity `binary_sensor.dw2_xxxx_door` will immediately reflect Open/Closed states.

---

## 📟 ESP32 Receiver Integration (`esp32_receiver/`)

For custom ESP32 central controllers (e.g., smart home heating thermostats), a lightweight header-only C++ receiver is provided in `esp32_receiver/dw2_ble_receiver.hpp`:

```cpp
#include "dw2_ble_receiver.hpp"

DW2Receiver receiver;

void onDoorEvent(const dw2_event_t &evt) {
    printf("Sensor [%s] | State: %s | Battery: %u mV | Duration: %u s\n",
           evt.mac, evt.is_open ? "OPEN" : "CLOSED", evt.battery_mv, evt.open_duration_sec);

    if (evt.is_open && evt.is_ajar) {
        // Door open > 45s: pause radiator valve in this room!
        radiator_turn_off();
    }
}

void app_main() {
    receiver.setCallback(onDoorEvent);
    receiver.begin();
}
```

---

## 📄 License & Credits

- **Firmware & CLI:** MIT License. Created by Bogdan & Antigravity (Google DeepMind).
- **Opulinks OPL1000 SDK:** Copyright (c) Opulinks Technology Ltd.
- **Hardware Design:** Sonoff / ITEAD Intelligent Systems Co., Ltd.
