# Sonoff DW2-Wi-Fi Custom Firmware & Local ESP32 Integration

Custom ultra-low-power, local-only firmware for the **Sonoff DW2-Wi-Fi** door/window sensor, built on the **Opulinks OPL1000 A2** SoC (ARM Cortex-M3).

Bypasses eWeLink and cloud dependencies completely by replacing Wi-Fi transmissions with **BLE non-connectable advertisement bursts**, communicating directly with an **ESP32** running smart home logic (e.g. multi-zone heating control).

---

## Highlights

- **100% Local & Cloud-Free:** No eWeLink, no CoolKit, no router dependency.
- **Ultra-Low Power:** Wi-Fi radio is never initialized. Device sleeps in `ps_smart_sleep` mode (~µA), waking up exclusively on Hall sensor level transitions and an RTC heartbeat timer.
- **Robust Delivery:** Broadcasts a short 1.5 s burst of BLE advertisements at 20–30 ms intervals per event.
- **Lossless Deduplication:** Each burst carries a monotonic event counter so the receiver ignores duplicate packets.
- **Battery Monitoring:** Reads actual battery voltage via the internal AuxADC on each wake-up.
- **Ready-to-Use ESP32 Receiver:** Header-only C++ driver using `NimBLE-Arduino` with passive scanning, designed to coexist seamlessly with Wi-Fi/MQTT on an ESP32 WROOM.

---

## Hardware Overview (PCB V1.2)

| Component | Designator | Role |
|---|---|---|
| **MCU/Radio** | `U1` | Opulinks OPL1000 A2 (ARM Cortex-M3 + BLE/Wi-Fi) |
| **SPI Flash** | `U2` | BoyaMicro 25Q80 (1 MB) |
| **UART Port** | `J2` | `3V3`, `GND`, `RXD`, `TXD` (programming & AT UART) |
| **Test Points** | `IO8`, `IO9`, `RST`, `ADC` | `IO8` is the 115200 bps Debug UART output |
| **Hall Sensor** | `H1` | Magnetic door/window contact (`IO5`) |
| **Pairing Button** | `S1` | Push button (`IO4`) |
| **Status LED** | `LED1` | Indicator LED (`IO21`) |
| **Battery ADC** | Internal | Voltage divider on `IO2` |
| **Power** | — | 2 × AAA batteries (3.0 V) |

### Pin Mapping & Polarity

- **Hall Sensor (`IO5`):**
  - `GPIO_LEVEL_LOW` = **Closed** (magnet present)
  - `GPIO_LEVEL_HIGH` = **Open** (magnet removed)
- **Button (`IO4`):** `GPIO_LEVEL_LOW` when pressed.
- **LED (`IO21`):** `GPIO_LEVEL_HIGH` = ON.
- **Debug UART (`IO8`):** 115200 bps, 8N1.

---

## BLE Protocol

Payload broadcast inside BLE `Manufacturer Specific Data` (AD Type `0xFF`):

```text
+-----------------------+----------+-------------------------------------------------+
| Field                 | Size     | Description                                     |
+-----------------------+----------+-------------------------------------------------+
| Company ID            | 2 bytes  | 0xFFFF (Test / custom)                          |
| Magic                 | 2 bytes  | 'D', 'W' (0x44, 0x57)                           |
| Protocol Version      | 1 byte   | 0x01                                            |
| Flags                 | 1 byte   | Bit 0: 1=Open, 0=Closed                         |
|                       |          | Bit 1: 1=Periodic Heartbeat                     |
|                       |          | Bit 2: 1=Button triggered                       |
| Event Counter         | 2 bytes  | uint16_t (little-endian), increments each event |
| Battery Voltage (mV)  | 2 bytes  | uint16_t (little-endian), e.g. 3197 = 3.197 V   |
+-----------------------+----------+-------------------------------------------------+
Total payload size: 10 bytes
```

---

## Project Structure

```text
.
├── firmware/                   # Custom OPL1000 A2 BLE-only firmware
│   ├── Makefile                # GCC build script (uses @args.rsp)
│   └── src/
│       ├── dw2_app.c           # Wake-up, BLE advertisement burst, debounce
│       ├── dw2_app.h
│       ├── dw2_config.h        # Pin assignments & timings
│       ├── dw2_protocol.h      # Shared BLE payload struct
│       ├── hal_pin_config_project.h
│       └── main_patch.c        # Patch entry point & pinmux initialization
├── esp32_receiver/             # ESP32 receiver component (PlatformIO)
│   ├── dw2_ble_receiver.hpp    # Header-only NimBLE receiver with deduplication
│   ├── main.cpp                # Example integration with smart-home-heating
│   └── platformio.ini
├── docs/
│   └── investigation.md        # Initial hardware & reverse-engineering notes
├── third_party/                # Cloned SDK and reference design (gitignored)
│   ├── OPL1000A2-SDK/
│   └── OPL1000A2-Door-Sensor-Coolkit-Cloud-HTTPS/
└── README.md
```

---

## Building the Firmware

### Prerequisites

1. **GNU Arm Embedded Toolchain** (`arm-none-eabi-gcc` 10+).
2. **GNU Make** (e.g. 4.4+).
3. The official Opulinks SDK cloned in `third_party/OPL1000A2-SDK`.

### Compilation

```bash
cd firmware
make all
```

Build output:
- `Output-GCC/dw2_ble.elf`
- `Output-GCC/dw2_ble.bin` (~70 KB)

---

## Flashing the DW2-Wi-Fi

### Wiring

Connect a **3.3 V USB-to-UART adapter** (e.g. CP2102, CH340) to `J2`:

| DW2 Pin (`J2`) | USB-UART Adapter | Notes |
|---|---|---|
| `GND` | `GND` | Common ground |
| `TXD` | `RX` | DW2 transmits to adapter |
| `RXD` | `TX` | Adapter transmits to DW2 |
| `3V3` | `3V3` | **Do not connect if AAA batteries are inserted!** |

> [!CAUTION]
> Never connect 5 V to `J2`! Never connect `3V3` from the adapter while batteries are inserted.

### Flashing via OPL1000 Download Tool

1. Launch `third_party/OPL1000A2-SDK/Tool/Download/download_RELEASE.exe`.
2. Select your USB-UART COM port at **115200 bps**.
3. Go to the **Pack** tab:
   - **Script:** `third_party/OPL1000A2-SDK/FW_Pack/PatchData.txt`
   - **M0 Bin:** `third_party/OPL1000A2-SDK/FW_Pack/opl1000_m0.bin`
   - **M3 Bin:** `firmware/Output-GCC/dw2_ble.bin`
   - Click **Pack** (generates `Patch/opl1000.bin`).
4. Go to the **Download** tab:
   - For temporary testing without touching flash: tick **Download to RAM**.
   - For permanent flashing: **untick** `Download to RAM`.
   - Click **Download**.
5. **Reset timing:** immediately after clicking *Download*, give an ultra-short pulse from `RST` to `GND` (a fraction of a second) to catch the ROM bootloader.
6. The progress bar will fill up and show: `Download ... successfully !`.

---

## ESP32 Receiver Integration

Add the header-only driver to your ESP32 project:

```cpp
#include "dw2_ble_receiver.hpp"

Dw2BleReceiver receiver([](const Dw2SensorEvent &ev) {
    Serial.printf("[DW2] %s | Event #%u | Vbat: %u mV | %s\n",
                  ev.address.toString().c_str(),
                  ev.event_counter,
                  ev.battery_mv,
                  ev.is_open ? "WINDOW OPEN" : "WINDOW CLOSED");

    if (ev.is_open) {
        // Turn off zone heating in smart-home-heating
    } else {
        // Resume heating
    }
});

void setup() {
    Serial.begin(115200);
    receiver.begin(); // Starts background passive BLE scan
}

void loop() {
    // FreeRTOS loop: WiFi, MQTT, PID heating logic run concurrently
}
```

PlatformIO dependency:
```ini
lib_deps =
    h2zero/NimBLE-Arduino @ ^1.4.2
```

---

## License

Firmware source and receiver integration are open source under the MIT License.
OPL1000 SDK components and libraries are copyright © Opulinks Technology Ltd.
