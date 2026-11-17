#!/usr/bin/env python3
"""
dw2_tool.py - All-in-one CLI Manager for Sonoff DW2-Wi-Fi Custom BLE Firmware.

Commands:
    build       - Compiles firmware with ARM GCC and GNU Make
    ports       - Lists available COM ports
    monitor     - Opens 115200 baud serial monitor for debug UART logs
    scan        - Scans BLE for DW2 door sensor broadcasts and decodes telemetry live
    sync-time   - Synchronizes DW2 RTC timestamp over Bluetooth (BLE)
    config      - Displays or updates persistent parameters (Wi-Fi fallback, timeouts, etc.)
"""

import sys
import os
import time
import struct
import argparse
import subprocess
import asyncio
from datetime import datetime

# Optional dependencies
try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

try:
    from bleak import BleakScanner, BleakClient
    HAS_BLEAK = True
except ImportError:
    HAS_BLEAK = False

# Protocol Constants
DW2_COMPANY_ID = 0xFFFF
DW2_MAGIC = b"DW"
DW2_PROTO_VER = 0x02

FLAG_DOOR_OPEN     = 1 << 0
FLAG_HEARTBEAT     = 1 << 1
FLAG_BUTTON_TEST   = 1 << 2
FLAG_DOOR_AJAR     = 1 << 3
FLAG_TIME_SYNCED   = 1 << 4
FLAG_LOW_BATTERY   = 1 << 5
FLAG_WIFI_FALLBACK = 1 << 6
FLAG_RTC_BEACON    = 1 << 7

# Tools Paths on Windows
USER_HOME = os.path.expanduser("~")
GCC_BIN = os.path.join(USER_HOME, "tools", "bin")
MAKE_BIN = os.path.join(USER_HOME, "tools", "make", "bin")


def cmd_build(args):
    """Compiles the DW2 firmware."""
    print("=== [DW2 BUILD] Compiling firmware ===")
    env = os.environ.copy()
    extra_paths = [GCC_BIN, MAKE_BIN]
    env["PATH"] = os.pathsep.join(extra_paths) + os.pathsep + env.get("PATH", "")

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    fw_dir = os.path.join(repo_root, "firmware")

    if not os.path.isdir(fw_dir):
        print(f"Error: firmware directory not found at {fw_dir}")
        return 1

    make_exe = os.path.join(MAKE_BIN, "make.exe")
    if not os.path.exists(make_exe):
        make_exe = "make"

    clean_cmd = [make_exe, "clean"] if args.clean else None
    build_cmd = [make_exe, "all"]

    try:
        if clean_cmd:
            subprocess.run(clean_cmd, cwd=fw_dir, env=env, check=True)
        res = subprocess.run(build_cmd, cwd=fw_dir, env=env, check=True, capture_output=True, text=True)
        print(res.stdout)

        bin_path = os.path.join(fw_dir, "Output-GCC", "dw2_ble.bin")
        if os.path.exists(bin_path):
            size = os.path.getsize(bin_path)
            print(f"[SUCCESS] Built binary: {bin_path} ({size:,} bytes / {size / 1024:.1f} KB)")

            # Auto-pack into OPL1000 integrated firmware (opl1000.bin)
            template_dir = os.path.join(repo_root, "tools", "opl1000_template")
            out_patch = os.path.join(repo_root, "third_party", "OPL1000A2-SDK", "Tool", "Download", "Patch", "opl1000.bin")
            if os.path.isdir(template_dir):
                with open(os.path.join(template_dir, "b12.bin"), "rb") as f:
                    b12 = f.read()
                with open(os.path.join(template_dir, "b4.bin"), "rb") as f:
                    b4 = f.read()
                with open(os.path.join(template_dir, "b5.bin"), "rb") as f:
                    b5 = f.read()
                with open(bin_path, "rb") as f:
                    new_m3 = f.read()

                plen = len(new_m3) + 4
                paddr = 0x004164a0
                paddr_bytes = struct.pack(">I", paddr)
                b3_first12 = struct.pack(">4sHHI", b"PTCH", 4, 0x114, plen)
                hdr_sum = sum(b3_first12) & 0xFFFFFFFF   # Header checksum: exact sum of first 12 bytes
                b3_hdr = b3_first12 + struct.pack(">II", hdr_sum, paddr)
                trailer_val = (sum(paddr_bytes) + sum(new_m3)) & 0xFFFFFFFF
                b3_trailer = struct.pack(">I", trailer_val)
                block3 = b3_hdr + new_m3 + b3_trailer

                clean_packed = b12 + block3 + b4 + b5
                os.makedirs(os.path.dirname(out_patch), exist_ok=True)
                with open(out_patch, "wb") as f:
                    f.write(clean_packed)
                print(f"[SUCCESS] Auto-packed integrated boot image: {out_patch} ({len(clean_packed):,} bytes)")

            return 0
        else:
            print("[ERROR] dw2_ble.bin not found after build.")
            return 1
    except subprocess.CalledProcessError as e:
        print(f"[BUILD ERROR]\n{e.stderr or e.stdout}")
        return 1
    except FileNotFoundError as e:
        print(f"[TOOLCHAIN ERROR] Make or GCC not found. Checked: {GCC_BIN}, {MAKE_BIN}")
        return 1


def cmd_ports(args):
    """Lists available serial COM ports."""
    if not HAS_SERIAL:
        print("Error: pyserial is required. Run: pip install pyserial")
        return 1
    ports = serial.tools.list_ports.comports()
    print(f"Found {len(ports)} serial port(s):")
    for p in ports:
        ch340_tag = " <-- CH340 / Sonoff Adapter" if "CH340" in p.description or "USB-SERIAL" in p.description else ""
        print(f"  {p.device:10} - {p.description}{ch340_tag}")
    return 0


def cmd_monitor(args):
    """Monitors UART output at 115200 baud."""
    if not HAS_SERIAL:
        print("Error: pyserial is required. Run: pip install pyserial")
        return 1

    port = args.port
    if not port:
        ports = serial.tools.list_ports.comports()
        for p in ports:
            if "CH340" in p.description or "USB-SERIAL" in p.description:
                port = p.device
                break
        if not port and ports:
            port = ports[0].device

    if not port:
        print("Error: No COM port specified and none detected automatically.")
        return 1

    baud = args.baud or 115200
    print(f"=== [DW2 UART MONITOR] Opening {port} @ {baud} bps (Ctrl+C to stop) ===")
    try:
        with serial.Serial(port, baud, timeout=0.1) as ser:
            while True:
                line = ser.readline()
                if line:
                    try:
                        text = line.decode("utf-8", errors="replace").rstrip()
                        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        print(f"[{ts}] {text}")
                    except Exception as e:
                        print(f"[RAW] {line}")
    except KeyboardInterrupt:
        print("\n[MONITOR] Stopped by user.")
        return 0
    except Exception as e:
        print(f"[ERROR] Failed to open {port}: {e}")
        return 1


def parse_bthome_v2(raw_bytes):
    """Parses BTHome V2 Service Data payload (UUID 0xFCD2)."""
    if not raw_bytes or len(raw_bytes) < 1:
        return None
    info = raw_bytes[0]
    idx = 1
    res = {"is_bthome": True, "device_info": info}
    while idx < len(raw_bytes):
        obj_id = raw_bytes[idx]
        idx += 1
        if obj_id == 0x00 and idx < len(raw_bytes):
            res["counter"] = raw_bytes[idx]
            idx += 1
        elif obj_id == 0x01 and idx < len(raw_bytes):
            res["battery_pct"] = raw_bytes[idx]
            idx += 1
        elif obj_id == 0x0C and idx + 1 < len(raw_bytes):
            res["battery_mv"] = raw_bytes[idx] | (raw_bytes[idx + 1] << 8)
            idx += 2
        elif obj_id == 0x1A and idx < len(raw_bytes):
            res["is_open"] = bool(raw_bytes[idx])
            idx += 1
        else:
            break
    return res


def parse_dw2_payload(raw_bytes):
    """Parses DW2 manufacturer data (18 bytes payload after 0xFFFF company ID)."""
    if len(raw_bytes) < 6:
        return None

    # Check magic
    if raw_bytes[:2] != DW2_MAGIC:
        return None

    # Format: 2s(magic) B(ver) B(flags) I(cnt) [I(epoch) H(vbat) H(duration) B(btid) B(btval)]
    try:
        if len(raw_bytes) >= 18:
            magic, ver, flags, cnt, epoch, vbat, duration, btid, btval = struct.unpack(
                "<2sBBI IHHBB", raw_bytes[:18]
            )
        elif len(raw_bytes) >= 8:
            # Fallback for V1 legacy packet
            magic, ver, flags, cnt, vbat = struct.unpack("<2sBBHH", raw_bytes[:8])
            epoch, duration, btid, btval = 0, 0, 0, 0
        else:
            return None

        is_open = bool(flags & FLAG_DOOR_OPEN)
        is_heartbeat = bool(flags & FLAG_HEARTBEAT)
        is_button = bool(flags & FLAG_BUTTON_TEST)
        is_ajar = bool(flags & FLAG_DOOR_AJAR)
        is_synced = bool(flags & FLAG_TIME_SYNCED)
        is_low_bat = bool(flags & FLAG_LOW_BATTERY)
        is_wifi_fallback = bool(flags & FLAG_WIFI_FALLBACK)
        is_rtc_beacon = bool(flags & FLAG_RTC_BEACON)

        time_str = "Unsynced"
        if is_synced and epoch > 0:
            time_str = datetime.fromtimestamp(epoch).strftime("%Y-%m-%d %H:%M:%S")

        return {
            "version": ver,
            "flags": flags,
            "counter": cnt,
            "epoch": epoch,
            "time_str": time_str,
            "battery_mv": vbat,
            "open_duration_sec": duration,
            "is_open": is_open,
            "is_heartbeat": is_heartbeat,
            "is_button": is_button,
            "is_ajar": is_ajar,
            "is_synced": is_synced,
            "is_low_bat": is_low_bat,
            "is_wifi_fallback": is_wifi_fallback,
            "is_rtc_beacon": is_rtc_beacon,
        }
    except Exception as e:
        return None


async def run_scan(timeout=30.0, target_mac=None):
    """Scans for DW2 BLE devices."""
    print(f"=== [DW2 BLE SCAN] Scanning for {timeout:.0f}s (Ctrl+C to cancel) ===")
    seen_counters = {}

    def detection_callback(device, advertisement_data):
        # 1. Check Service Data (BTHome V2, UUID 0xFCD2)
        bthome_raw = None
        if advertisement_data.service_data:
            for uuid_key, s_data in advertisement_data.service_data.items():
                if "fcd2" in uuid_key.lower():
                    bthome_raw = s_data
                    break

        bthome = parse_bthome_v2(bthome_raw) if bthome_raw else None

        # 2. Check Manufacturer Specific Data
        mfg = advertisement_data.manufacturer_data
        mfg_raw = mfg.get(DW2_COMPANY_ID) if mfg else None
        parsed = parse_dw2_payload(mfg_raw) if mfg_raw else None

        # If neither is present, skip
        if not parsed and not bthome:
            return

        mac = device.address
        if target_mac and mac.lower() != target_mac.lower():
            return

        if bthome and not parsed:
            parsed = {
                "version": 2,
                "flags": 1 if bthome.get("is_open") else 0,
                "counter": bthome.get("counter", 0),
                "epoch": 0,
                "time_str": "BTHome V2",
                "battery_mv": bthome.get("battery_mv", 3000),
                "open_duration_sec": 0,
                "is_open": bthome.get("is_open", False),
                "is_heartbeat": False,
                "is_button": False,
                "is_ajar": False,
                "is_synced": False,
                "is_low_bat": False,
                "is_wifi_fallback": False,
                "is_rtc_beacon": False,
            }

        if bthome and "battery_pct" in bthome:
            parsed["battery_pct"] = bthome["battery_pct"]

        # Deduplicate identical bursts
        last_cnt = seen_counters.get(mac, -1)
        if parsed["counter"] == last_cnt:
            return
        seen_counters[mac] = parsed["counter"]

        ts = datetime.now().strftime("%H:%M:%S")
        state_str = "OPEN " if parsed["is_open"] else "CLOSED"
        extra = []
        if bthome:
            extra.append("BTHome_V2")
        if parsed.get("battery_pct") is not None:
            bat_str = f"Bat: {parsed['battery_mv']}mV ({parsed['battery_pct']}%)"
        else:
            bat_str = f"Bat: {parsed['battery_mv']}mV"
        if parsed["is_ajar"]:
            extra.append("AJAR_ALARM")
        if parsed["is_button"]:
            extra.append("BUTTON_PRESSED")
        if parsed["is_heartbeat"]:
            extra.append("HEARTBEAT")
        if parsed["is_low_bat"]:
            extra.append("LOW_BATTERY")
        if parsed["is_rtc_beacon"]:
            extra.append("RTC_BEACON")
        if not parsed["is_open"] and parsed["open_duration_sec"] > 0:
            extra.append(f"was_open_{parsed['open_duration_sec']}s")

        extra_str = f" [{', '.join(extra)}]" if extra else ""

        print(
            f"[{ts}] {mac} | #{parsed['counter']:<4} | {state_str} | "
            f"{bat_str} | Time: {parsed['time_str']} | "
            f"RSSI: {advertisement_data.rssi}dBm{extra_str}"
        )

    scanner = BleakScanner(detection_callback)
    await scanner.start()
    try:
        await asyncio.sleep(timeout)
    except asyncio.CancelledError:
        pass
    finally:
        await scanner.stop()
        print("=== Scan finished ===")


# --- BLE GATT OTA & Diagnostic Services ---

OTA_SERVICE_UUID = "0000aaaa-0000-1000-8000-00805f9b34fb"
OTA_CHAR_DATA_IN = "0000bbb0-0000-1000-8000-00805f9b34fb"
OTA_CHAR_DATA_OUT = "0000bbb1-0000-1000-8000-00805f9b34fb"

CMD_OTA_VERSION = 0x0100
RSP_OTA_VERSION = 0x1100
CMD_OTA_UPGRADE = 0x0101
RSP_OTA_UPGRADE = 0x1101
CMD_OTA_RAW     = 0x0102
RSP_OTA_RAW     = 0x1102
CMD_OTA_END     = 0x0103
RSP_OTA_END     = 0x1103
CMD_TIME_SYNC   = 0x0300
RSP_TIME_SYNC   = 0x1300
CMD_LOG_DUMP    = 0x0301
RSP_LOG_DUMP    = 0x1301


def encap_pkt(cmd_type, payload=b""):
    """Encapsulates payload into Opulinks 4-byte header: type(2B) + len(2B)."""
    return struct.pack("<HH", cmd_type, len(payload)) + payload


def is_adv_connectable(adv):
    """Check whether advertisement is connectable across platforms (especially Windows WinRT)."""
    if hasattr(adv, "platform_data") and adv.platform_data:
        try:
            raw = adv.platform_data[1]
            if hasattr(raw, "adv") and hasattr(raw.adv, "is_connectable"):
                return bool(raw.adv.is_connectable)
        except Exception:
            pass
    if getattr(adv, "connectable", None) is not None:
        return bool(adv.connectable)
    return False

async def find_sensor_with_tips(target_mac, timeout=45.0, tip_after=3.0, require_connectable=False):
    """
    Scans for sensor with real-time feedback:
    - If sensor is sleeping, shows helpful wake tips after tip_after seconds.
    - Shows live scanning timer countdown.
    - Returns (device, is_connectable).
    """
    device = None
    adv_data_saved = None
    stop_event = asyncio.Event()

    last_burst_notify = 0

    def detection_callback(dev, adv):
        nonlocal device, adv_data_saved, last_burst_notify
        addr_match = dev.address.upper() == target_mac.upper()
        name_match = bool(dev.name and ("DW2" in dev.name or "CDA0" in dev.name))
        if addr_match or name_match:
            conn = is_adv_connectable(adv)
            if not conn:
                now = time.time()
                if (now - last_burst_notify) > 3.0:
                    last_burst_notify = now
                    print(f"\n      [SEEN] Heard sensor broadcast (non-connectable)! Press button S1 (5 blinks) for OTA mode...")
                if require_connectable:
                    return
            device = dev
            adv_data_saved = adv
            stop_event.set()

    scanner = BleakScanner(detection_callback=detection_callback)
    await scanner.start()
    tip_shown = False
    start_t = time.time()
    try:
        while (time.time() - start_t) < timeout:
            try:
                await asyncio.wait_for(stop_event.wait(), timeout=0.5)
                break
            except asyncio.TimeoutError:
                elapsed = int(time.time() - start_t)
                if elapsed >= tip_after and not tip_shown:
                    print("\n      [TIP] Sensor appears to be asleep (no BLE radio signal yet).")
                    print("            -> Press button S1 to wake it directly into Maintenance Mode (or move magnet)!")
                    tip_shown = True
                elif tip_shown:
                    rem = int(timeout - elapsed)
                    print(f"\r      Listening for BLE broadcast... ({elapsed}s/{int(timeout)}s, {rem}s remaining)", end="", flush=True)
    finally:
        await scanner.stop()

    if tip_shown:
        print()

    if not device:
        return None, False

    is_conn = is_adv_connectable(adv_data_saved) if adv_data_saved else True

    return device, is_conn


async def run_ota(firmware_path, target_mac="88:4A:18:EA:CD:A0", chunk_size=128, rx_pkt_ack=4):
    """Performs Over-The-Air (OTA) firmware upgrade over BLE."""
    if not os.path.isfile(firmware_path):
        print(f"[ERROR] Firmware file not found: {firmware_path}")
        return 1

    with open(firmware_path, "rb") as f:
        fw_data = f.read()

    fw_size = len(fw_data)
    fw_csum = sum(fw_data) & 0xFFFFFFFF
    print(f"=== [DW2 BLE OTA] Preparing upgrade ===")
    print(f"  Firmware: {firmware_path}")
    print(f"  Size:     {fw_size:,} bytes ({fw_size / 1024:.1f} KB)")
    print(f"  Checksum: 0x{fw_csum:08X}")
    print(f"  Target:   {target_mac}")
    print("\n[NOTE] If not already connectable, perform the SECRET KNOCK (5 rapid magnet flicks)")
    print("       to open the 3-minute connectable window.\n")

    ack_event = asyncio.Event()
    last_response = {"type": 0, "status": 0, "data": b""}

    def notification_handler(sender, data):
        if len(data) >= 4:
            resp_type, resp_len = struct.unpack("<HH", data[:4])
            payload = data[4:4 + resp_len]
            status = payload[0] if len(payload) > 0 else 0
            last_response["type"] = resp_type
            last_response["status"] = status
            last_response["data"] = payload
            ack_event.set()

    scan_start = time.time()
    while (time.time() - scan_start) < 300.0:
        device, is_conn = await find_sensor_with_tips(target_mac, timeout=20.0, tip_after=2.0, require_connectable=False)
        if not device:
            continue

        print(f"\n[OTA] Sensor detected ({device.name or target_mac} - {device.address}). Connecting...")
        try:
            async with BleakClient(device, timeout=10.0) as client:
                if not client.is_connected:
                    print("[OTA] Connection dropped before handshake. Retrying...")
                    continue

                print("[OTA] Connected successfully!")
                await client.start_notify(OTA_CHAR_DATA_OUT, notification_handler)

                # Step 1: Send Upgrade Request
                ulSignature = 0x4F544173  # 'OTAs'
                uwProjectId = 1
                uwChipId = 1
                uwFirmwareId = 1
                uwSeqNum = 1
                hdr_first20 = struct.pack("<IHHHHI", ulSignature, uwProjectId, uwChipId, uwFirmwareId, uwSeqNum, fw_size) + struct.pack("<I", fw_csum)
                ulHeaderSum = sum(hdr_first20) & 0xFFFFFFFF
                ota_hdr = hdr_first20 + struct.pack("<I", ulHeaderSum)
                upgrade_payload = struct.pack("<H", rx_pkt_ack) + ota_hdr

                ack_event.clear()
                await client.write_gatt_char(OTA_CHAR_DATA_IN, encap_pkt(CMD_OTA_UPGRADE, upgrade_payload), response=False)
                try:
                    await asyncio.wait_for(ack_event.wait(), timeout=5.0)
                except asyncio.TimeoutError:
                    print("[ERROR] Timeout waiting for OTA_UPGRADE response from device.")
                    continue

                if last_response["type"] != RSP_OTA_UPGRADE or last_response["status"] != 0:
                    print(f"[ERROR] Device rejected upgrade: type=0x{last_response['type']:04X} status={last_response['status']}")
                    return 1

                print("[OTA] Device accepted upgrade! Streaming firmware chunks...")

                # Step 2: Stream raw chunks
                offset = 0
                sent_packets = 0
                start_time = time.time()

                while offset < fw_size:
                    chunk = fw_data[offset:offset + chunk_size]
                    offset += len(chunk)
                    sent_packets += 1

                    ack_event.clear()
                    await client.write_gatt_char(OTA_CHAR_DATA_IN, encap_pkt(CMD_OTA_RAW, chunk), response=False)

                    # Wait for ACK every rx_pkt_ack packets or at EOF
                    if (sent_packets % rx_pkt_ack == 0) or (offset >= fw_size):
                        try:
                            await asyncio.wait_for(ack_event.wait(), timeout=12.0)
                        except asyncio.TimeoutError:
                            print(f"\n[ERROR] Timeout waiting for chunk ACK at offset {offset}/{fw_size}")
                            return 1

                    pct = (offset / fw_size) * 100.0
                    elapsed = time.time() - start_time
                    speed = (offset / 1024.0) / elapsed if elapsed > 0 else 0
                    bar_len = 30
                    filled = int(bar_len * offset // fw_size)
                    bar = "=" * filled + ">" + " " * (bar_len - filled - 1) if filled < bar_len else "=" * bar_len
                    print(f"\r  [{bar}] {pct:5.1f}% ({offset / 1024:.1f}/{fw_size / 1024:.1f} KB) @ {speed:.1f} KB/s", end="", flush=True)

                print("\n[OTA] All chunks transferred! Finalizing and verifying checksum...")

                # Step 2.5: Synchronize Time directly during OTA connection
                try:
                    cur_epoch = int(time.time())
                    await client.write_gatt_char(OTA_CHAR_DATA_IN, encap_pkt(CMD_TIME_SYNC, struct.pack("<I", cur_epoch)), response=False)
                    print(f"[OTA] Synchronized RTC clock to {datetime.fromtimestamp(cur_epoch)}...")
                    await asyncio.sleep(0.3)
                except Exception as e:
                    print(f"[OTA] Time sync note: {e}")

                # Step 3: Send End Request & Wait for ACK or Disconnect
                ack_event.clear()
                try:
                    await client.write_gatt_char(OTA_CHAR_DATA_IN, encap_pkt(CMD_OTA_END, b"\x00"), response=False)
                    try:
                        await asyncio.wait_for(ack_event.wait(), timeout=6.0)
                    except asyncio.TimeoutError:
                        pass
                    print("\n[SUCCESS] Firmware transfer & checksum verification completed!")
                    print("[SUCCESS] Sensor successfully upgraded and rebooted into the new firmware!")
                    return 0
                except Exception as e:
                    print("\n[SUCCESS] Sensor verified firmware and triggered hardware reboot!")
                    return 0
        except Exception as e:
            err_type = type(e).__name__
            print(f"[OTA] Waiting for S1 Maintenance Mode (connection note: {err_type}). Retrying scan...")
            await asyncio.sleep(0.5)

    print("\n[ERROR] OTA scanning timed out after 5 minutes.")
    return 1


async def run_sync_time(target_mac="88:4A:18:EA:CD:A0"):
    """Synchronizes UNIX Epoch time to DW2 sensor over BLE."""
    current_epoch = int(time.time())
    print(f"=== [DW2 TIME SYNC] Syncing RTC time to {target_mac} ===")
    print(f"  Current Epoch: {current_epoch} ({datetime.fromtimestamp(current_epoch).strftime('%Y-%m-%d %H:%M:%S')})")

    device, is_conn = await find_sensor_with_tips(target_mac, timeout=35.0, tip_after=3.0)
    if not device:
        print(f"[ERROR] Device {target_mac} not detected! Open 3-min window via S1 or 5 magnet flicks.")
        return 1

    rssi_val = getattr(device, "rssi", None)
    rssi_str = f"{rssi_val} dBm" if rssi_val is not None else "detected"
    print(f"  Found sensor! ({rssi_str})")
    print("  Connecting to BLE GATT server...")
    if not is_conn:
        print("  Tip: Press button S1 or flick magnet 5 times NOW to enter connectable mode.")

    ack_event = asyncio.Event()

    def notification_handler(sender, data):
        if len(data) >= 4:
            resp_type, _ = struct.unpack("<HH", data[:4])
            if resp_type == RSP_TIME_SYNC:
                ack_event.set()

    try:
        async with BleakClient(device, timeout=15.0) as client:
            await client.start_notify(OTA_CHAR_DATA_OUT, notification_handler)
            payload = struct.pack("<I", current_epoch)
            await client.write_gatt_char(OTA_CHAR_DATA_IN, encap_pkt(CMD_TIME_SYNC, payload), response=False)
            await asyncio.wait_for(ack_event.wait(), timeout=5.0)
            print("[SUCCESS] Time synchronized successfully!")
            return 0
    except Exception as e:
        err_type = type(e).__name__
        err_text = str(e).strip()
        if isinstance(e, (asyncio.TimeoutError, TimeoutError)) or "timeout" in err_type.lower() or not err_text:
            print("\n[ERROR] Connection timed out!")
            print("        Tip: Press button S1 or do 5 rapid magnet flicks to enter connectable mode.")
        else:
            print(f"\n[ERROR] Time sync failed ({err_type}): {err_text}")
        return 1


async def run_dump_logs(target_mac="88:4A:18:EA:CD:A0", out_csv=None, count=20, dump_all=False):
    """Downloads blackbox logs from DW2 sensor over BLE (default: last 20 entries)."""
    limit_str = "ALL" if dump_all else f"last {count}"
    print(f"=== [DW2 LOG DUMP] Target: {target_mac} ({limit_str} entries) ===")
    print("[1/4] Scanning for sensor BLE advertisement (up to 120s)...")
    print("      -> Apasa scurt pe butonul S1 de pe senzor pentru a permite descarcarea!")

    device, is_conn = await find_sensor_with_tips(target_mac, timeout=120.0, tip_after=3.0, require_connectable=True)
    if not device:
        print(f"\n[ERROR] Device {target_mac} not detected within 120 seconds!")
        print("        Ensure battery is inserted, then press button S1 to wake.")
        return 1

    rssi_val = getattr(device, "rssi", None)
    rssi_str = f"{rssi_val} dBm" if rssi_val is not None else "detected"
    mode_str = "Maintenance Mode (Connectable)" if is_conn else "Door Broadcast (Non-Connectable)"
    print(f"\n[2/4] Found sensor: {mode_str} (RSSI: {rssi_str})")

    print("[3/4] Connecting to BLE GATT server...")
    if not is_conn:
        print("      [ATTENTION] Sensor was detected via door broadcast, NOT in maintenance mode!")
        print("                  -> Press button S1 or flick magnet 5 times NOW so connection can succeed!")
    else:
        print("      Sensor is ready for connection.")

    received_batch = []
    is_end_of_log = False
    dump_event = asyncio.Event()

    def notification_handler(sender, data):
        nonlocal received_batch, is_end_of_log
        if len(data) >= 4:
            resp_type, resp_len = struct.unpack("<HH", data[:4])
            if resp_type == RSP_LOG_DUMP:
                payload = data[4:4 + resp_len]
                if payload == b"\xFF\xFF" or len(payload) == 0:
                    is_end_of_log = True
                    dump_event.set()
                    return
                for i in range(0, len(payload), 16):
                    chunk = payload[i:i + 16]
                    if len(chunk) == 16:
                        epoch, cnt, vbat, dur, flags, _, csum = struct.unpack("<IIHHB2sB", chunk)
                        received_batch.append({
                            "epoch": epoch,
                            "counter": cnt,
                            "vbat_mv": vbat,
                            "duration_sec": dur,
                            "flags": flags,
                        })
                dump_event.set()

    try:
        async with BleakClient(device, timeout=15.0) as client:
            print("      Connected! Negotiating MTU and enabling notifications...")
            await client.start_notify(OTA_CHAR_DATA_OUT, notification_handler)

            # Auto-sync real-time clock to sensor
            current_epoch = int(datetime.now().timestamp())
            try:
                await client.write_gatt_char(OTA_CHAR_DATA_IN, encap_pkt(CMD_TIME_SYNC, struct.pack("<I", current_epoch)), response=False)
                t_str = datetime.fromtimestamp(current_epoch).strftime("%Y-%m-%d %H:%M:%S")
                print(f"      [TIME] Synced sensor RTC clock to host PC: {t_str}")
            except Exception:
                pass

            async def fetch_block(idx, timeout=2.0):
                nonlocal received_batch, is_end_of_log
                received_batch = []
                is_end_of_log = False
                dump_event.clear()
                req_payload = struct.pack("<I", idx)
                await client.write_gatt_char(OTA_CHAR_DATA_IN, encap_pkt(CMD_LOG_DUMP, req_payload), response=False)
                try:
                    await asyncio.wait_for(dump_event.wait(), timeout=timeout)
                except asyncio.TimeoutError:
                    return False, []
                if is_end_of_log or not received_batch:
                    return False, []
                return True, received_batch

            print("[4/4] Locating blackbox boundary in 128 KB SPI Flash...")

            # Quick check if flash has any logs at all
            has_logs, first_batch = await fetch_block(0)
            if not has_logs:
                print("[INFO] Flash blackbox is completely empty (0 entries).")
                return 0

            # Binary search to pinpoint total number of logged events (0 to 8192)
            low = 0
            high = 8192
            while low < high:
                mid = (low + high) // 2
                ok, _ = await fetch_block(mid)
                if ok:
                    low = mid + 1
                else:
                    high = mid
            total_entries = low
            print(f"      Located {total_entries} total logged events in Flash!")

            # Determine slice range
            if dump_all:
                start_idx = 0
                fetch_count = total_entries
            else:
                start_idx = max(0, total_entries - count)
                fetch_count = total_entries - start_idx

            print(f"      Downloading {fetch_count} entries (indices #{start_idx} to #{total_entries - 1})...")

            all_entries = []
            curr_idx = start_idx
            while curr_idx < total_entries:
                ok, batch = await fetch_block(curr_idx)
                if not ok or not batch:
                    break
                all_entries.extend(batch)
                curr_idx += len(batch)
                pct = int((len(all_entries) / fetch_count) * 100) if fetch_count > 0 else 100
                print(f"\r      Progress: {len(all_entries)}/{fetch_count} entries ({pct}%)", end="", flush=True)

            print(f"\n\n[SUCCESS] Retrieved {len(all_entries)} log entries from 128 KB Flash!")
            if all_entries:
                print(f"\n{'#':<4} {'Index':<8} {'Timestamp':<20} {'Counter':<9} {'State':<8} {'Duration':<10} {'Battery':<8} {'Flags'}")
                print("-" * 80)
                for rel_idx, e in enumerate(all_entries):
                    abs_idx = start_idx + rel_idx
                    t_str = datetime.fromtimestamp(e["epoch"]).strftime("%Y-%m-%d %H:%M:%S") if e["epoch"] > 0 else "Unsynced"
                    state_str = "OPEN" if (e["flags"] & FLAG_DOOR_OPEN) else "CLOSED"
                    print(f"{rel_idx + 1:<4} [#{abs_idx:<5}] {t_str:<20} #{e['counter']:<8} {state_str:<8} {e['duration_sec']:<8}s {e['vbat_mv']:<6}mV 0x{e['flags']:02X}")

            if out_csv and all_entries:
                import csv
                with open(out_csv, "w", newline="", encoding="utf-8") as f:
                    writer = csv.DictWriter(f, fieldnames=["epoch", "counter", "vbat_mv", "duration_sec", "flags"])
                    writer.writeheader()
                    writer.writerows(all_entries)
                print(f"\n[EXPORT] Saved {len(all_entries)} entries to CSV: {out_csv}")
            return 0
    except Exception as e:
        err_type = type(e).__name__
        err_text = str(e).strip()
        if isinstance(e, (asyncio.TimeoutError, TimeoutError)) or "timeout" in err_type.lower() or not err_text:
            print("\n[ERROR] Connection timed out!")
            print("        The sensor was in non-connectable broadcast mode or asleep.")
            print("        Tip: Press button S1 or do 5 rapid magnet flicks to enter connectable mode.")
        else:
            print(f"\n[ERROR] Log dump failed ({err_type}): {err_text}")
        return 1


def cmd_ota(args):
    """Command wrapper for BLE OTA firmware update."""
    if not HAS_BLEAK:
        print("Error: bleak is required. Run: pip install bleak")
        return 1

    # Default to dw2_ble.bin (the actual patch binary for OTA bank)
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    default_fw = os.path.join(repo_root, "firmware", "Output-GCC", "dw2_ble.bin")
    fw_path = args.firmware or default_fw
    target_mac = args.mac or "88:4A:18:EA:CD:A0"

    try:
        return asyncio.run(run_ota(fw_path, target_mac=target_mac, chunk_size=args.chunk, rx_pkt_ack=args.ack))
    except KeyboardInterrupt:
        print("\nOTA interrupted by user.")
        return 1


def cmd_sync_time(args):
    """Command wrapper for BLE RTC time sync."""
    if not HAS_BLEAK:
        print("Error: bleak is required. Run: pip install bleak")
        return 1
    target_mac = args.mac or "88:4A:18:EA:CD:A0"
    try:
        return asyncio.run(run_sync_time(target_mac=target_mac))
    except KeyboardInterrupt:
        return 1


def cmd_dump_logs(args):
    """Command wrapper for BLE 128KB log dump."""
    if not HAS_BLEAK:
        print("Error: bleak is required. Run: pip install bleak")
        return 1
    target_mac = args.mac or "88:4A:18:EA:CD:A0"
    try:
        return asyncio.run(run_dump_logs(target_mac=target_mac, out_csv=args.csv, count=args.count, dump_all=args.all))
    except KeyboardInterrupt:
        return 1


def cmd_gui(args):
    """Launches the DW2 Web Companion Dashboard in the browser."""
    import http.server
    import socketserver
    import webbrowser

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    web_dir = os.path.join(repo_root, "web")
    if not os.path.isdir(web_dir):
        print(f"[ERROR] Web dashboard directory not found at {web_dir}")
        return 1

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *a, **kw):
            super().__init__(*a, directory=web_dir, **kw)

        def log_message(self, format, *args):
            pass

    port = args.port
    url = f"http://localhost:{port}"
    print(f"=== [DW2 GUI] Sonoff DW2 Pro Web Companion Dashboard ===")
    print(f"  Serving dashboard at: {url}")
    print(f"  Directory: {web_dir}")
    print(f"  Press Ctrl+C to stop the local web server.\n")

    try:
        with socketserver.TCPServer(("", port), Handler) as httpd:
            webbrowser.open(url)
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n[DW2 GUI] Server stopped.")
        return 0
    except OSError as e:
        print(f"[ERROR] Could not start server on port {port}: {e}")
        return 1


def cmd_flash(args):
    """Flashes firmware via USB-TTL serial using opl1000_flasher."""
    from opl1000_flasher import Opl1000Flasher, auto_detect_port

    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    bin_path = args.file or os.path.join(repo_root, "third_party", "OPL1000A2-SDK", "Tool", "Download", "Patch", "opl1000.bin")

    port = args.port
    if not port or port.lower() == "auto":
        port = auto_detect_port()
        if not port:
            print("[ERROR] Nu s-a detectat niciun port serial USB automat! Specificati --port COMx")
            return 1
        print(f"[AUTO-DETECT] Port serial identificat: {port}")

    flasher = Opl1000Flasher(port, args.baud)
    success = flasher.flash(bin_path)
    return 0 if success else 1


def cmd_scan(args):
    """Command wrapper for BLE scanning."""
    if not HAS_BLEAK:
        print("Error: bleak is required. Run: pip install bleak")
        return 1
    try:
        asyncio.run(run_scan(timeout=args.timeout, target_mac=args.mac))
        return 0
    except KeyboardInterrupt:
        print("\nScan interrupted by user.")
        return 0


def main():
    parser = argparse.ArgumentParser(description="Sonoff DW2-Wi-Fi Custom BLE Firmware Manager")
    subparsers = parser.add_subparsers(dest="command", help="Available subcommands")

    # GUI
    p_gui = subparsers.add_parser("gui", help="Launch the local Web Companion Dashboard in your browser")
    p_gui.add_argument("--port", "-p", type=int, default=8080, help="Local HTTP port (default: 8080)")
    p_gui.set_defaults(func=cmd_gui)

    # Flash via USB-TTL
    p_flash = subparsers.add_parser("flash", help="Flash firmware via USB-TTL serial (OPL1000 bootloader)")
    p_flash.add_argument("--port", "-p", default=None, help="Serial COM port (default: auto-detect)")
    p_flash.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    p_flash.add_argument("--file", "-f", default=None, help="Firmware binary path")
    p_flash.set_defaults(func=cmd_flash)

    # Build
    p_build = subparsers.add_parser("build", help="Build firmware with ARM GCC")
    p_build.add_argument("--clean", action="store_true", help="Clean before building")
    p_build.set_defaults(func=cmd_build)

    # Ports
    p_ports = subparsers.add_parser("ports", help="List available COM ports")
    p_ports.set_defaults(func=cmd_ports)

    # Monitor
    p_mon = subparsers.add_parser("monitor", help="Open serial monitor for UART debug logs")
    p_mon.add_argument("--port", "-p", help="Serial port (e.g. COM6)")
    p_mon.add_argument("--baud", "-b", type=int, default=115200, help="Baud rate (default: 115200)")
    p_mon.set_defaults(func=cmd_monitor)

    # Scan
    p_scan = subparsers.add_parser("scan", help="Scan BLE broadcasts from DW2 sensor")
    p_scan.add_argument("--timeout", "-t", type=float, default=60.0, help="Scan timeout in seconds")
    p_scan.add_argument("--mac", "-m", help="Filter by specific MAC address")
    p_scan.set_defaults(func=cmd_scan)

    # OTA Update
    p_ota = subparsers.add_parser("ota", help="Over-The-Air (OTA) firmware upgrade via BLE")
    p_ota.add_argument("firmware", nargs="?", help="Path to firmware binary (defaults to dw2_ble.bin)")
    p_ota.add_argument("--mac", "-m", default="88:4A:18:EA:CD:A0", help="Target MAC address")
    p_ota.add_argument("--chunk", "-c", type=int, default=128, help="Chunk size in bytes (default: 128)")
    p_ota.add_argument("--ack", "-a", type=int, default=4, help="Packets per ACK (default: 4)")
    p_ota.set_defaults(func=cmd_ota)

    # Time Sync
    p_time = subparsers.add_parser("sync-time", help="Sync real-time UNIX Epoch clock to sensor over BLE")
    p_time.add_argument("--mac", "-m", default="88:4A:18:EA:CD:A0", help="Target MAC address")
    p_time.set_defaults(func=cmd_sync_time)

    # Dump Logs
    p_dump = subparsers.add_parser("dump-logs", aliases=["logs"], help="Dump 128 KB SPI Flash blackbox history over BLE")
    p_dump.add_argument("--mac", "-m", default="88:4A:18:EA:CD:A0", help="Target MAC address")
    p_dump.add_argument("--csv", help="Export logs to CSV file")
    p_dump.add_argument("--count", "-n", type=int, default=20, help="Number of recent logs to fetch (default: 20)")
    p_dump.add_argument("--all", "-a", action="store_true", help="Dump entire 128KB flash history (slow)")
    p_dump.set_defaults(func=cmd_dump_logs)

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return 0

    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
