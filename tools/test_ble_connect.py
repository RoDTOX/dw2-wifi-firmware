import asyncio
import sys
from bleak import BleakScanner, BleakClient

UUID_SVC_OTA = "0000aaaa-0000-1000-8000-00805f9b34fb"
UUID_CHAR_DATA_IN = "0000bbb0-0000-1000-8000-00805f9b34fb"
UUID_CHAR_DATA_OUT = "0000bbb1-0000-1000-8000-00805f9b34fb"

async def test_connect(mac="88:4A:18:EA:CD:A0"):
    print(f"Scanning for {mac} (DW2-CDA0)...")
    print("TIP: Press button S1 on the sensor (LED will blink 5 times) to enter connectable mode!")
    
    device = None
    for attempt in range(15):
        devices = await BleakScanner.discover(timeout=2.0)
        for d in devices:
            if d.address.upper() == mac.upper() or (d.name and "DW2" in d.name):
                print(f"-> Found device: {d.name} [{d.address}]")
                device = d
                break
        if device:
            break
        print(f"Waiting for sensor advertisement (attempt {attempt+1}/15)...")

    if not device:
        print("[FAIL] Sensor not detected within 30s. Did you press button S1?")
        return False

    print(f"\nAttempting GATT connection to {device.address}...")
    try:
        async with BleakClient(device, timeout=15.0) as client:
            print(f"[OK] Connected! Is connected: {client.is_connected}")
            print("Discovering GATT services...")
            for s in client.services:
                print(f"  Service: {s.uuid} ({s.description})")
                for c in s.characteristics:
                    print(f"    Char: {c.uuid} [{','.join(c.properties)}]")
            print("\n[SUCCESS] Web/BLE GATT connection is fully functional!")
            return True
    except Exception as e:
        print(f"[ERROR] GATT Connection failed: {type(e).__name__}: {e}")
        return False

if __name__ == "__main__":
    mac = sys.argv[1] if len(sys.argv) > 1 else "88:4A:18:EA:CD:A0"
    asyncio.run(test_connect(mac))
