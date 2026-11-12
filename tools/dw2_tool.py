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
                hdr_sum = (sum(b3_first12) + 8) & 0xFFFFFFFF   # Header checksum: sum(12 bytes) + 8
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
        mfg = advertisement_data.manufacturer_data
        if DW2_COMPANY_ID not in mfg:
            return

        raw = mfg[DW2_COMPANY_ID]
        parsed = parse_dw2_payload(raw)
        if not parsed:
            return

        mac = device.address
        if target_mac and mac.lower() != target_mac.lower():
            return

        # Deduplicate identical bursts
        last_cnt = seen_counters.get(mac, -1)
        if parsed["counter"] == last_cnt:
            return
        seen_counters[mac] = parsed["counter"]

        ts = datetime.now().strftime("%H:%M:%S")
        state_str = "OPEN " if parsed["is_open"] else "CLOSED"
        extra = []
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
            f"Bat: {parsed['battery_mv']}mV | Time: {parsed['time_str']} | "
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

    args = parser.parse_args()
    if not args.command:
        parser.print_help()
        return 0

    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
