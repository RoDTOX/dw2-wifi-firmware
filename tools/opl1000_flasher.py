"""
OPL1000 UART Flash Tool (Python 3) - Fixed timing
Direct implementation matching Opulinks patch_download.py
"""

import os
import sys
import time
import struct
import serial
import serial.tools.list_ports

CONSTANT_WAIT_RESP_TIME = 6.0
FLASH_ERASE_WAIT_TIME = 30.0


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
                return p.device
    for p in ports:
        if "USB" in (p.hwid or "").upper():
            return p.device
    non_com1 = [p.device for p in ports if p.device != "COM1"]
    if non_com1:
        return non_com1[0]
    return ports[0].device


class Opl1000Flasher:
    SECTOR_SIZE = 4096
    CHUNK_SIZE = 256

    RSP_CHECK = b"<CHECK>"
    RSP_START = b"<START>"
    RSP_ACK = b"<ACK>"

    REQ_ERASE_START = b"\xfe2"
    REQ_WRITE_START = b"\xfe1"
    REQ_WRITE_HEADER = b"netlinkc"

    def __init__(self, port, baud=115200):
        self.port = port
        self.baud = baud
        self.ser = None

    def open(self):
        try:
            self.ser = serial.Serial(
                self.port,
                self.baud,
                timeout=0.01,
                bytesize=8,
                parity="N",
                stopbits=1,
            )
            return True
        except Exception as e:
            print(f"[ERROR] Nu pot deschide {self.port}: {e}")
            return False

    def close(self):
        if self.ser:
            try:
                self.ser.flush()
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    def _tx(self, data):
        if not self.ser:
            return False
        self.ser.write(data)
        self.ser.flush()
        return True

    def _wait_keyword(self, keyword, timeout=CONSTANT_WAIT_RESP_TIME):
        if not self.ser:
            return False
        rx_buf = b""
        t0 = time.time()
        while time.time() - t0 < timeout:
            try:
                chunk = self.ser.read(64)
                if chunk:
                    rx_buf += chunk
                    if keyword in rx_buf:
                        return True
                    if b"<NACK>" in rx_buf:
                        print(f"\n[ERROR] NACK de la ROM: {rx_buf}")
                        return False
            except Exception:
                time.sleep(0.05)
        if rx_buf:
            print(f"\n[DEBUG] Primit in loc de {keyword}: {rx_buf}")
        return False

    def flash(self, bin_path, flash_start_addr=0):
        if not os.path.isfile(bin_path):
            print(f"[ERROR] Fisierul nu exista: {bin_path}")
            return False

        with open(bin_path, "rb") as f:
            fw_bytes = f.read()

        fw_size = len(fw_bytes)
        patch_num = (fw_size + self.CHUNK_SIZE - 1) // self.CHUNK_SIZE
        sector_num = (fw_size + self.SECTOR_SIZE - 1) // self.SECTOR_SIZE

        print(f"=== OPL1000 Flasher ===")
        print(f"Port:       {self.port} @ {self.baud} baud")
        print(f"Fisier:     {bin_path} ({fw_size:,} bytes / {fw_size / 1024:.1f} KB)")
        print(f"Sectoare:   {sector_num} | Chunk-uri: {patch_num}")
        print("--------------------------------------------------")
        print(">> Astept reset senzor... (scoate si baga firul 3V3 sau atinge RST la GND)")

        if not self.open():
            return False

        try:
            self.ser.reset_input_buffer()

            # 1. Asteptam <CHECK> de la ROM la pornire
            t_start = time.time()
            caught = False
            last_dot = time.time()
            while time.time() - t_start < 60.0:
                try:
                    chunk = self.ser.read(1) if self.ser else b""
                    if chunk and b"<" in chunk:
                        rest = self.ser.read(6)
                        if b"CHECK>" in rest:
                            caught = True
                            break
                except Exception:
                    time.sleep(0.08)
                    try:
                        self.close()
                        self.open()
                    except Exception:
                        pass

                if time.time() - last_dot >= 1.0:
                    sys.stdout.write(".")
                    sys.stdout.flush()
                    last_dot = time.time()

            if not caught:
                print("\n[TIMEOUT] Nu am primit <CHECK> in 60 secunde.")
                return False

            print("\n[OK] S-a detectat OPL1000 ROM bootloader!")

            # 2. Trimite comanda de stergere IMEDIAT la primirea <CHECK>
            self._tx(self.REQ_ERASE_START)

            if not self._wait_keyword(self.RSP_START, timeout=FLASH_ERASE_WAIT_TIME):
                print("[ERROR] Nu am primit <START> la stergere flash.")
                return False

            start_sector = flash_start_addr // self.SECTOR_SIZE
            self._tx(struct.pack(">I", start_sector))
            self._tx(struct.pack(">I", sector_num))

            print(">> Stergere flash in desfasurare...", end="", flush=True)
            if not self._wait_keyword(self.RSP_ACK, timeout=FLASH_ERASE_WAIT_TIME):
                print("\n[ERROR] Timeout <ACK> la stergere.")
                return False
            print(" [OK]")

            # Dupa stergere, trecem direct la initializarea scrierii
            time.sleep(0.1)
            self._wait_keyword(self.RSP_CHECK, timeout=0.5)

            # 3. Initializare scriere
            self._tx(self.REQ_WRITE_START)
            if not self._wait_keyword(self.RSP_START, timeout=CONSTANT_WAIT_RESP_TIME):
                print("[ERROR] Nu am primit <START> la scriere.")
                return False

            self._tx(self.REQ_WRITE_HEADER)
            self._tx(struct.pack(">I", patch_num))

            if not self._wait_keyword(self.RSP_ACK, timeout=CONSTANT_WAIT_RESP_TIME):
                print("[ERROR] Nu am primit <ACK> la header.")
                return False

            # 4. Scriere blocuri (Speed-Up mode)
            print(">> Scriere firmware in desfasurare:")
            curr_addr = flash_start_addr
            t_write_start = time.time()

            for i in range(patch_num):
                offset = i * self.CHUNK_SIZE
                chunk = fw_bytes[offset:offset + self.CHUNK_SIZE]
                checksum = sum(chunk) & 0xFFFFFFFF

                pkt = struct.pack(">II", curr_addr, len(chunk)) + chunk + struct.pack(">I", checksum)
                self._tx(pkt)

                if not self._wait_keyword(self.RSP_ACK, timeout=CONSTANT_WAIT_RESP_TIME):
                    print(f"\n[ERROR] Timeout <ACK> la bloc {i + 1}/{patch_num}")
                    return False

                time.sleep(0.005)  # 5ms safe margin for SPI flash page commit
                curr_addr += len(chunk)
                pct = int((i + 1) * 100 / patch_num)
                bar = "=" * (pct // 4) + "-" * (25 - (pct // 4))
                print(f"\r[{bar}] {pct}% ({i + 1}/{patch_num})", end="", flush=True)

            dur = time.time() - t_write_start
            print(f"\n[SUCCES] Firmware scris cu succes in {dur:.1f} secunde!")
            print(">> Gata! Poti scoate firele TTL si pune bateriile.")
            return True

        finally:
            self.close()


def main():
    import argparse
    parser = argparse.ArgumentParser(description="OPL1000 Serial Flasher")
    parser.add_argument("-p", "--port", default=None, help="Port serial COM (implicit: auto-detect)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Viteza baud (implicit: 115200)")
    parser.add_argument("-f", "--file", default="third_party/OPL1000A2-SDK/Tool/Download/Patch/opl1000.bin", help="Cale binar")
    args = parser.parse_args()

    port = args.port
    if not port:
        port = auto_detect_port()
        if port:
            print(f"[AUTO-DETECT] Port serial identificat: {port}")
        else:
            print("[ERROR] Niciun port serial USB detectat!")
            sys.exit(1)

    flasher = Opl1000Flasher(port, args.baud)
    success = flasher.flash(args.file)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
