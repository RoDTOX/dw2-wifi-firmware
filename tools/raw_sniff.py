"""
Real-time BLE Raw Frame Sniffer for Sonoff DW2 (88:4A:18:EA:CD:A0)
Captures and displays all advertisement frames, hex payloads, and decoded BTHome/DW2 data.
"""

import sys
import time
import asyncio
from bleak import BleakScanner

TARGET_MAC = "88:4A:18:EA:CD:A0"


def hex_dump(data: bytes) -> str:
    return " ".join(f"{b:02X}" for b in data)


def decode_bthome_v2(data: bytes):
    if len(data) < 4:
        return f"Unknown ({len(data)}B): {hex_dump(data)}"
    # BTHome V2 header: 0x44 (device info)
    info_byte = data[0]
    idx = 1
    objs = []
    while idx < len(data):
        obj_id = data[idx]
        idx += 1
        if obj_id == 0x00 and idx < len(data):  # Packet ID (1B)
            objs.append(f"pkt_id={data[idx]}")
            idx += 1
        elif obj_id == 0x01 and idx < len(data):  # Battery % (1B)
            objs.append(f"bat={data[idx]}%")
            idx += 1
        elif obj_id == 0x0C and idx + 1 < len(data):  # Voltage (2B, mV)
            mv = data[idx] | (data[idx + 1] << 8)
            objs.append(f"vbat={mv}mV ({mv/1000.0:.3f}V)")
            idx += 2
        elif obj_id == 0x1A and idx < len(data):  # Door (1B)
            st = "OPEN" if data[idx] else "CLOSED"
            objs.append(f"door={st} ({data[idx]})")
            idx += 1
        else:
            objs.append(f"obj_0x{obj_id:02X}")
            break
    return " | ".join(objs)


def callback(device, adv):
    if device.address.upper() != TARGET_MAC.upper():
        # Check by name just in case MAC is masked or random
        if not (adv.local_name and "DW2" in adv.local_name):
            return

    now = time.strftime("%H:%M:%S")
    name = adv.local_name or "<no name>"
    rssi = adv.rssi

    print(f"\n[{now}] === RAW FRAME from {device.address} (RSSI: {rssi} dBm, Name: '{name}') ===")

    # Service Data
    if adv.service_data:
        for uuid, sdata in adv.service_data.items():
            print(f"  Service Data [{uuid}]:")
            print(f"    HEX:     {hex_dump(sdata)}")
            if "fcd2" in uuid.lower():
                print(f"    BTHome:  {decode_bthome_v2(sdata)}")
    else:
        print("  Service Data: None")

    # Manufacturer Data
    if adv.manufacturer_data:
        for comp_id, mdata in adv.manufacturer_data.items():
            print(f"  Manufacturer Data [0x{comp_id:04X}]:")
            print(f"    HEX:     {hex_dump(mdata)}")
    else:
        print("  Manufacturer Data: None")

    # Service UUIDs
    if adv.service_uuids:
        print(f"  Service UUIDs: {adv.service_uuids}")

    print("=" * 65, flush=True)


async def main():
    print(f"=== Listening for raw BLE frames from {TARGET_MAC} ===")
    print("Muta magnetul pe senzor (aproape/departe) sau apasa butonul S1.")
    print("Apasa Ctrl+C pentru a opri.")

    scanner = BleakScanner(detection_callback=callback)
    await scanner.start()
    try:
        while True:
            await asyncio.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        await scanner.stop()
        print("\nSniffer oprit.")


if __name__ == "__main__":
    asyncio.run(main())
