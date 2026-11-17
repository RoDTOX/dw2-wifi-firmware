"""
Continuous Auto-Retry OPL1000 Flasher
Keeps listening on COM7 and auto-retries on every reset until 100% SUCCESS.
"""

import os
import sys
import time
import struct
import serial
import serial.tools.list_ports

CHUNK_SIZE = 256
SECTOR_SIZE = 4096


def auto_detect_port():
    """Detects available USB-UART serial ports automatically."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        return None
    usb_keywords = ["CH340", "CP210", "FTDI", "PL2303", "USB-SERIAL", "USB SERIAL", "UART"]
    for p in ports:
        desc = (p.description or "").upper()
        hwid = (p.hwid or "").upper()
        for kw in usb_keywords:
            if kw in desc or kw in hwid:
                return p.device, p.description
    for p in ports:
        if "USB" in (p.hwid or "").upper():
            return p.device, p.description
    non_com1 = [p for p in ports if p.device != "COM1"]
    if non_com1:
        return non_com1[0].device, non_com1[0].description
    return ports[0].device, ports[0].description


def run_flasher(port="auto", baud=115200, bin_path="third_party/OPL1000A2-SDK/Tool/Download/Patch/opl1000.bin"):
    if not port or port.lower() == "auto":
        detected = auto_detect_port()
        while not detected:
            print("\r[SCAN] Niciun adaptor USB-UART gasit. Conecteaza cablul USB...", end="", flush=True)
            time.sleep(1.0)
            detected = auto_detect_port()
        port, desc = detected
        print(f"[AUTO-DETECT] Port serial identificat: {port} ({desc})")

    with open(bin_path, "rb") as f:
        fw_bytes = f.read()

    fw_size = len(fw_bytes)
    patch_num = (fw_size + CHUNK_SIZE - 1) // CHUNK_SIZE
    sector_num = (fw_size + SECTOR_SIZE - 1) // SECTOR_SIZE

    print(f"\n==================================================")
    print(f"=== OPL1000 Continuous Auto-Flasher ===")
    print(f"Port:     {port} @ {baud}")
    print(f"Firmware: {bin_path} ({fw_size:,} B / {patch_num} chunks)")
    print(f"Mod:      Asculta continuu si rescrie automat la reset")
    print(f"==================================================")

    attempt = 1
    while True:
        ser = None
        try:
            try:
                ser = serial.Serial(port, baud, timeout=0.02)
            except Exception:
                time.sleep(0.5)
                continue

            print(f"\n[Incercarea #{attempt}] Astept semnal senzor (reset / baterie)...", flush=True)

            # 1. Asteptam semnalul de start <CHECK>
            caught = False
            t0 = time.time()
            last_dot = time.time()
            while time.time() - t0 < 60.0:
                try:
                    c = ser.read(1)
                    if c == b"<":
                        tag = ser.read(6)
                        if b"CHECK>" in tag:
                            caught = True
                            break
                except Exception:
                    break
                if time.time() - last_dot >= 1.0:
                    sys.stdout.write(".")
                    sys.stdout.flush()
                    last_dot = time.time()

            if not caught:
                ser.close()
                attempt += 1
                continue

            print("\n[OK] S-a detectat OPL1000 ROM bootloader!")

            # 2. Stergere flash
            ser.write(b"\xfe2")
            ser.flush()

            # Asteptam <START>
            rx = b""
            t_w = time.time()
            start_ok = False
            while time.time() - t_w < 5.0:
                ch = ser.read(32)
                if ch:
                    rx += ch
                    if b"<START>" in rx:
                        start_ok = True
                        break
            if not start_ok:
                print(f"[RETRY] Nu am primit <START> la erase (primit: {rx})")
                ser.close()
                attempt += 1
                time.sleep(0.5)
                continue

            # Trimite numarul de sectoare
            ser.write(struct.pack(">II", 0, sector_num))
            ser.flush()
            print(">> Stergere flash in desfasurare...", end="", flush=True)

            rx = b""
            t_w = time.time()
            ack_ok = False
            while time.time() - t_w < 15.0:
                ch = ser.read(32)
                if ch:
                    rx += ch
                    if b"<ACK>" in rx:
                        ack_ok = True
                        break
            if not ack_ok:
                print(f" [TIMEOUT ERASE ACK: {rx}]")
                ser.close()
                attempt += 1
                time.sleep(0.5)
                continue
            print(" [OK]")

            # 3. Initializare scriere
            time.sleep(0.05)
            ser.write(b"\xfe1")
            ser.flush()

            rx = b""
            t_w = time.time()
            start_ok = False
            while time.time() - t_w < 5.0:
                ch = ser.read(32)
                if ch:
                    rx += ch
                    if b"<START>" in rx:
                        start_ok = True
                        break
            if not start_ok:
                print(f"[RETRY] Nu am primit <START> la write (primit: {rx})")
                ser.close()
                attempt += 1
                time.sleep(0.5)
                continue

            # Header scriere
            ser.write(b"netlinkc" + struct.pack(">I", patch_num))
            ser.flush()

            rx = b""
            t_w = time.time()
            ack_ok = False
            while time.time() - t_w < 5.0:
                ch = ser.read(32)
                if ch:
                    rx += ch
                    if b"<ACK>" in rx:
                        ack_ok = True
                        break
            if not ack_ok:
                print(f"[RETRY] Nu am primit <ACK> la header (primit: {rx})")
                ser.close()
                attempt += 1
                time.sleep(0.5)
                continue

            # 4. Scriere blocuri cu pauza mica de siguranta
            print(">> Scriere firmware in desfasurare:")
            curr_addr = 0
            t_write = time.time()
            write_failed = False

            for i in range(patch_num):
                offset = i * CHUNK_SIZE
                chunk = fw_bytes[offset:offset + CHUNK_SIZE]
                csum = sum(chunk) & 0xFFFFFFFF
                pkt = struct.pack(">II", curr_addr, len(chunk)) + chunk + struct.pack(">I", csum)

                ser.write(pkt)
                ser.flush()

                # Asteptam <ACK>
                rx = b""
                t_w = time.time()
                got_ack = False
                while time.time() - t_w < 4.0:
                    ch = ser.read(32)
                    if ch:
                        rx += ch
                        if b"<ACK>" in rx:
                            got_ack = True
                            break
                        if b"<NACK>" in rx:
                            break

                if not got_ack:
                    print(f"\n[RETRY] Eroare/Timeout la chunk {i + 1}/{patch_num} (primit: {rx})")
                    write_failed = True
                    break

                curr_addr += len(chunk)
                pct = int((i + 1) * 100 / patch_num)
                bar = "=" * (pct // 4) + "-" * (25 - (pct // 4))
                print(f"\r[{bar}] {pct}% ({i + 1}/{patch_num})", end="", flush=True)
                time.sleep(0.003)  # 3ms SPI write settle

            if write_failed:
                ser.close()
                attempt += 1
                time.sleep(0.5)
                continue

            dur = time.time() - t_write
            print(f"\n\n==================================================")
            print(f"[SUCCES 100%] Firmware scris in {dur:.1f}s!")
            print(f"Senzorul este gata. Poti scoate firele si monta bateriile!")
            print(f"==================================================")
            ser.close()
            return True

        except Exception as e:
            if ser:
                try:
                    ser.close()
                except Exception:
                    pass
            time.sleep(0.5)
            attempt += 1


if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "auto"
    run_flasher(port)
