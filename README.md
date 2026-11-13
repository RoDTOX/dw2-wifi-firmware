<div align="center">

# 🚪 Sonoff DW2-Wi-Fi — Custom BLE Firmware

<p align="center">
  <img src="https://readme-typing-svg.demolab.com?font=Fira+Code&weight=600&size=20&pause=1000&color=41BDF5&center=true&vCenter=true&width=650&lines=Sonoff+DW2-Wi-Fi+%E2%86%92+100%25+Local+BLE+Powerhouse;BTHome+V2+Auto-Discovery+%E2%80%A2+Dual-Bank+OTA;~2+Years+Battery+Life+(~30%C2%B5A+Deep+Sleep);128+KB+SPI+Flash+Blackbox+Recorder;Zero+Cloud+%E2%80%A2+Zero+eWeLink+%E2%80%A2+Instant+Speed" alt="Typing SVG" />
</p>

### ⚡ Liberate your sensor into a 100% local, ultra-low-power BTHome V2 powerhouse

[![Firmware License](https://img.shields.io/badge/License-MIT-blue.svg?style=for-the-badge)](LICENSE)
[![Hardware SoC](https://img.shields.io/badge/SoC-Opulinks%20OPL1000%20A2-orange.svg?style=for-the-badge&logo=arm)](https://www.opulinks.com/)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-BTHome%20V2-41BDF5.svg?style=for-the-badge&logo=homeassistant&logoColor=white)](https://bthome.io/)
[![Battery Life](https://img.shields.io/badge/Battery-~2%20Years%20(30µA)-brightgreen.svg?style=for-the-badge&logo=battery)](https://github.com/RoDTOX/dw2-wifi-firmware)
[![OTA Updates](https://img.shields.io/badge/OTA-Dual--Bank%20Wireless-purple.svg?style=for-the-badge&logo=bluetooth)](https://github.com/RoDTOX/dw2-wifi-firmware)
[![Build Status](https://img.shields.io/badge/GCC%2014.2-Passing%20(0%20errors)-success.svg?style=for-the-badge&logo=gnu)](https://github.com/RoDTOX/dw2-wifi-firmware)

<br/>

<!-- Tech Stack Tags -->
<p align="center">
  <img src="https://img.shields.io/badge/C-00599C?style=flat-square&logo=c&logoColor=white" alt="C" />
  <img src="https://img.shields.io/badge/ARM_Cortex--M3-0091BD?style=flat-square&logo=arm&logoColor=white" alt="ARM" />
  <img src="https://img.shields.io/badge/Bluetooth_LE_5.0-0082FC?style=flat-square&logo=bluetooth&logoColor=white" alt="BLE" />
  <img src="https://img.shields.io/badge/Home_Assistant-41BDF5?style=flat-square&logo=homeassistant&logoColor=white" alt="Home Assistant" />
  <img src="https://img.shields.io/badge/Python_3.13-3776AB?style=flat-square&logo=python&logoColor=white" alt="Python" />
  <img src="https://img.shields.io/badge/GCC_14.2-A42E2B?style=flat-square&logo=gnu&logoColor=white" alt="GCC" />
  <img src="https://img.shields.io/badge/FreeRTOS-82BE27?style=flat-square&logo=freertos&logoColor=white" alt="FreeRTOS" />
</p>

<!-- Quick Navigation Bar -->
<p align="center">
  <a href="#-key-features"><img src="https://img.shields.io/badge/✨_Features-21262d?style=for-the-badge&logoColor=white" alt="Features" /></a>
  <a href="#-quick-comparison-stock-vs-custom"><img src="https://img.shields.io/badge/⚖️_Comparison-21262d?style=for-the-badge&logoColor=white" alt="Comparison" /></a>
  <a href="#-hardware-specifications-pcb-v12"><img src="https://img.shields.io/badge/📐_Hardware-21262d?style=for-the-badge&logoColor=white" alt="Hardware" /></a>
  <a href="#-led-feedback--hardware-button-gestures"><img src="https://img.shields.io/badge/🎛️_Gestures-21262d?style=for-the-badge&logoColor=white" alt="Gestures" /></a>
  <a href="#-quick-start--first-time-flash"><img src="https://img.shields.io/badge/🚀_Quick_Start-21262d?style=for-the-badge&logoColor=white" alt="Quick Start" /></a>
  <a href="#-wireless-ota-upgrades-over-ble"><img src="https://img.shields.io/badge/📡_Wireless_OTA-21262d?style=for-the-badge&logoColor=white" alt="OTA" /></a>
  <a href="#-cli-tool-reference-toolsdw2_toolpy"><img src="https://img.shields.io/badge/🛠️_CLI_Tool-21262d?style=for-the-badge&logoColor=white" alt="CLI" /></a>
  <a href="#-home-assistant-integration"><img src="https://img.shields.io/badge/🏠_Home_Assistant-21262d?style=for-the-badge&logoColor=white" alt="HA" /></a>
</p>

---

</div>

## 🌟 Key Features

<table>
  <tr>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/PRIVACY-100%25%20LOCAL-success?style=flat-square" alt="Privacy" /><br/>🛡️ 100% Cloud-Free & Local</h4>
      <p>Operates entirely within your local Bluetooth perimeter. <b>Zero eWeLink accounts</b>, zero cloud servers, and zero internet dependency. Eliminates Wi-Fi pairing dropouts and vendor lock-in forever.</p>
    </td>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/BATTERY-~2%20YEARS-brightgreen?style=flat-square" alt="Battery" /><br/>🔋 Extreme Battery Runtime</h4>
      <p>Wi-Fi radio is permanently switched off in firmware. Deep sleep runs at <b>~30 µA</b> via hardware <code>ps_smart_sleep</code> and 32.768 kHz RTC compensation, delivering <b>~2 years of life</b> on standard AAA cells.</p>
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/INTEGRATION-BTHOME%20V2-blue?style=flat-square" alt="BTHome" /><br/>🏠 Native BTHome V2 Auto-Discovery</h4>
      <p>Broadcasts standard <b>BTHome V2 Door objects (<code>0x1A</code>)</b>. Instantly auto-discovered by <b>Home Assistant</b>, <b>ESPHome BLE Proxies</b>, and <b>Shelly BLU</b> gateways with zero manual YAML.</p>
    </td>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/FIRMWARE-DUAL--BANK%20OTA-purple?style=flat-square" alt="OTA" /><br/>📡 Fail-Safe Wireless BLE OTA</h4>
      <p>True wireless firmware upgrades over Bluetooth GATT (<code>0xAAAA</code>). Dual-Bank architecture toggles between <b>Bank 0</b> and <b>Bank 1</b> with full CRC validation and automatic fallback if interrupted.</p>
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/TELEMETRY-AJAR%20ALERT-red?style=flat-square" alt="Telemetry" /><br/>⏱️ Open Duration & Ajar Warning</h4>
      <p>Measures exact open duration in seconds and emits <code>was_open_Xs</code> upon closing. If left open <b>> 45 seconds</b>, emits a dedicated high-priority ajar alert every 15s to prevent energy waste.</p>
    </td>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/STORAGE-128%20KB%20FLASH-orange?style=flat-square" alt="Storage" /><br/>💾 128 KB Flash Blackbox Recorder</h4>
      <p>Dedicated on-chip SPI Flash circular log stores up to <b>8,192 historical events</b> with timestamps, battery mV, and flags. <b>Survives battery swaps</b> and dumps to CSV via CLI in ~0.5s via binary search.</p>
    </td>
  </tr>
  <tr>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/HARDWARE-MULTI--TIER%20S1-blueviolet?style=flat-square" alt="Button" /><br/>🎛️ Multi-Tier Physical Button Actions</h4>
      <p>The onboard <code>S1</code> button serves multiple roles: <b>&lt; 5s</b> opens OTA mode, <b>8–10s</b> executes SoC Hard Reboot, and <b>≥ 15s</b> executes a complete factory format of the blackbox flash memory.</p>
    </td>
    <td width="50%" valign="top">
      <h4><img src="https://img.shields.io/badge/IDENTITY-DYNAMIC%20MAC-informational?style=flat-square" alt="Identity" /><br/>🏷️ Unique Case-Matched BLE Name</h4>
      <p>Advertises dynamically as <b><code>DW2-XXXX</code></b> (e.g. <code>DW2-CDA0</code>) using the device's hardware factory MAC address, perfectly matching the serial sticker printed on the outer plastic casing.</p>
    </td>
  </tr>
</table>

---

## ⚖️ Quick Comparison: Stock vs Custom

## 📐 Hardware Specifications (PCB V1.2)

```text
       +---------------------------------------------+
       | [J2 Header: 3V3  TXD  RXD  GND]             |
       |                                             |
       |   +-----------------------+     [LED1]      |
       |   |   Opulinks OPL1000    |                 |
       |   |  Cortex-M3 + M0 SoC   |     [S1 Button] |
       |   +-----------------------+                 |
       |                                             |
       |   [BoyaMicro 25Q80 Flash]       [H1 Hall]   |
       +---------------------------------------------+
```

| Component | Designator | Specification / Role |
|---|---|---|
| **SoC** | `U1` | Opulinks OPL1000 A2 (ARM Cortex-M3 @ 44–176 MHz + Cortex-M0) |
| **SPI NOR Flash** | `U2` | BoyaMicro 25Q80 (1 MB / 8 Mbit SPI Flash) |
| **Magnetic Sensor** | `H1` | Hall Effect switch on `IO5` (Active Low: Magnet present = Closed) |
| **Maintenance Button** | `S1` | Tactile push button on `IO4` (Active Low) |
| **Indicator LED** | `LED1` | Surface-mount LED on `IO21` (Active High) |
| **Battery AuxADC** | Internal | Internal SoC rail measurement + OTP factory regression calibration |
| **Serial Header** | `J2` | 4-pin 2.54mm pitch: `3V3`, `TXD`, `RXD`, `GND` |
| **Debug UART** | `IO8` | Single-wire 115200 bps Debug UART TX (kernel boot logs) |
| **Power Source** | — | 2 × AAA Alkaline / NiMH batteries (nominal 3.0 V) |

---

## 🎛️ LED Feedback & Hardware Button Gestures

### LED Indications

| Pattern | Duration / Cadence | Meaning |
|---|---|---|
| **3 Short Blinks** | 80 ms ON / 120 ms OFF | System cold boot / Hardware reset |
| **1 Blink** | 80 ms ON | Door Opened |
| **2 Blinks** | 80 ms ON / 120 ms OFF | Door Closed |
| **5 Rapid Blinks** | 50 ms ON / 70 ms OFF | Maintenance / BLE Connectable OTA Mode Activated (3 min) |
| **1 Single Blink (at 8s)** | 100 ms ON | Cue: *"Release now to Hard Reboot"* |
| **10 Rapid Blinks** | 40 ms ON / 40 ms OFF | Hard Reboot sequence executing |
| **Solid LED (at 15s)** | Continuous ON | Cue: *"Release now to Factory Format Blackbox"* |

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

```text
0x00000000 +-------------------------------------------------------+
           | 2nd Stage Boot Agent (opl1000_ota_loader.bin)   (12 KB)|
0x00003000 +-------------------------------------------------------+
           | OTA Header 1 (Bank 0 Descriptor & Checksum)      (4 KB)|
0x00004000 +-------------------------------------------------------+
           | OTA Header 2 (Bank 1 Descriptor & Checksum)      (4 KB)|
0x00005000 +-------------------------------------------------------+
           | Dual-Bank 0 (Primary Application Firmware)     (228 KB)|
0x0003E000 +-------------------------------------------------------+
           | Dual-Bank 1 (OTA Staging Firmware)             (228 KB)|
0x00077000 +-------------------------------------------------------+
           | Opulinks RF Calibration, MAC & NVS Storage     (420 KB)|
0x000E0000 +-------------------------------------------------------+
           | DW2 Blackbox Logger (32 x 4 KB Sectors, 8,192) (128 KB)|
0x00100000 +-------------------------------------------------------+
```

---

## 🚀 Quick Start & First-Time Flash

The initial transition from stock eWeLink firmware to custom firmware requires connecting a USB-to-UART adapter once to install the **Dual-Bank Bootloader**. Afterward, all future firmware updates are performed **100% wirelessly over BLE**.

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

### 2. Flashing the Initial Bootloader Image

1. Open `third_party/OPL1000A2-SDK/Tool/Download/download_RELEASE.exe`.
2. Select your USB COM port, Baud rate `115200`.
3. Choose the generated complete bootloader package:
   ```text
   third_party/OPL1000A2-SDK/Tool/Download/Patch/opl1000.bin
   ```
4. Click **Start** in the download tool *before* powering the board.
5. Connect 3.3V power (or momentarily ground `RST`). The ROM bootloader will catch the sync pattern and flash the full image in ~15 seconds.
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
# 1. Compile firmware binary with GCC
python tools/dw2_tool.py build

# 2. Live BLE packet monitor & door event scanner
python tools/dw2_tool.py scan --timeout 30

# 3. Synchronize RTC internal clock from host PC
python tools/dw2_tool.py sync-time

# 4. Dump the last 20 historical blackbox entries (takes ~0.5s via binary search)
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

- **Firmware & CLI:** MIT License. Created by [Bogdan](https://github.com/RoDTOX) & Antigravity (Google DeepMind).
- **Opulinks OPL1000 SDK:** Copyright (c) Opulinks Technology Ltd.
- **Hardware Design:** Sonoff / ITEAD Intelligent Systems Co., Ltd.
