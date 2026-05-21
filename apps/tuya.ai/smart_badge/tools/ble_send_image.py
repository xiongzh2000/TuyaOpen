#!/usr/bin/env python3
"""
BLE Image Transfer Tool for Smart Badge

Send JPEG images to the smart badge via BLE custom GATT service.

Usage:
    pip install bleak Pillow
    python ble_send_image.py photo.jpg
    python ble_send_image.py photo.jpg --device "tuya"
    python ble_send_image.py photo.jpg --resize 466

Protocol:
    1. Connect to device, negotiate MTU
    2. Write START command [0x01][total_size:4B LE] to Control characteristic
    3. Wait for READY notification [0x81][status:1B][mtu:2B LE]
    4. Write raw JPEG data chunks to Data characteristic (WRITE_NO_RSP)
    5. Write FINISH command [0x02] to Control characteristic
    6. Wait for DONE notification [0x82][status:1B][received:4B LE]
"""

import argparse
import asyncio
import struct
import sys
from pathlib import Path

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("Install bleak: pip install bleak")
    sys.exit(1)

SVC_UUID  = "12340001-5678-1234-5678-123456789abc"
CTRL_UUID = "12340002-5678-1234-5678-123456789abc"
DATA_UUID = "12340003-5678-1234-5678-123456789abc"

CMD_START  = 0x01
CMD_FINISH = 0x02
RSP_READY  = 0x81
RSP_DONE   = 0x82

STATUS_NAMES = {0: "OK", 1: "SIZE_MISMATCH", 2: "NO_MEMORY", 3: "SAVE_FAILED"}


def resize_image(path: str, max_size: int) -> bytes:
    """Resize image to fit within max_size x max_size and return JPEG bytes."""
    try:
        from PIL import Image
        import io
    except ImportError:
        print("Install Pillow for resize: pip install Pillow")
        sys.exit(1)

    img = Image.open(path)
    img.thumbnail((max_size, max_size), Image.LANCZOS)
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=85)
    return buf.getvalue()


async def send_image(device_filter: str, image_path: str, resize: int = 0):
    data = (
        resize_image(image_path, resize)
        if resize > 0
        else Path(image_path).read_bytes()
    )
    total_size = len(data)
    print(f"Image: {image_path} ({total_size} bytes)")

    # Scan for device
    print(f"Scanning for BLE device (filter: '{device_filter}')...")
    target = None
    devices = await BleakScanner.discover(timeout=10.0)
    for d in devices:
        name = d.name or ""
        if device_filter.lower() in name.lower():
            target = d
            break
        for uuid in (d.metadata.get("uuids") or []):
            if SVC_UUID in uuid.lower():
                target = d
                break
        if target:
            break

    if not target:
        print("Device not found. Available devices:")
        for d in devices:
            if d.name:
                print(f"  {d.address}  {d.name}")
        return

    print(f"Connecting to {target.name} ({target.address})...")
    notify_event = asyncio.Event()
    notify_data = bytearray()

    def on_notify(_handle, value: bytearray):
        nonlocal notify_data
        notify_data = value
        notify_event.set()

    async with BleakClient(target, timeout=15.0) as client:
        mtu = client.mtu_size
        print(f"Connected. MTU={mtu}")

        await client.start_notify(CTRL_UUID, on_notify)

        # START
        start_cmd = struct.pack("<BI", CMD_START, total_size)
        notify_event.clear()
        await client.write_gatt_char(CTRL_UUID, start_cmd, response=True)
        print("START sent, waiting for READY...")

        await asyncio.wait_for(notify_event.wait(), timeout=5.0)
        if len(notify_data) >= 2 and notify_data[0] == RSP_READY:
            status = notify_data[1]
            if status != 0:
                print(f"Device rejected: {STATUS_NAMES.get(status, status)}")
                return
            dev_mtu = struct.unpack_from("<H", notify_data, 2)[0] if len(notify_data) >= 4 else mtu
            print(f"Device READY, device MTU={dev_mtu}")
        else:
            print(f"Unexpected response: {notify_data.hex()}")
            return

        # DATA chunks (WRITE_NO_RSP)
        chunk_size = min(mtu - 3, dev_mtu - 3, 509)
        if chunk_size < 20:
            chunk_size = 20
        sent = 0
        chunk_count = 0
        while sent < total_size:
            end = min(sent + chunk_size, total_size)
            chunk = data[sent:end]
            await client.write_gatt_char(DATA_UUID, chunk, response=False)
            sent = end
            chunk_count += 1
            pct = sent * 100 // total_size
            print(f"\r  Sending: {pct}% ({sent}/{total_size})", end="", flush=True)
        print(f"\n  Sent {chunk_count} chunks")

        # Small delay for device to process last writes
        await asyncio.sleep(0.2)

        # FINISH
        notify_event.clear()
        await client.write_gatt_char(CTRL_UUID, bytes([CMD_FINISH]), response=True)
        print("FINISH sent, waiting for DONE...")

        await asyncio.wait_for(notify_event.wait(), timeout=10.0)
        if len(notify_data) >= 2 and notify_data[0] == RSP_DONE:
            status = notify_data[1]
            received = struct.unpack_from("<I", notify_data, 2)[0] if len(notify_data) >= 6 else 0
            status_name = STATUS_NAMES.get(status, f"UNKNOWN({status})")
            print(f"Result: {status_name}, device received {received} bytes")
            if status == 0:
                print("Image transfer successful!")
            else:
                print("Transfer failed.")
        else:
            print(f"Unexpected response: {notify_data.hex()}")

        await client.stop_notify(CTRL_UUID)


def main():
    parser = argparse.ArgumentParser(description="Send image to Smart Badge via BLE")
    parser.add_argument("image", help="Path to JPEG/PNG image file")
    parser.add_argument("--device", "-d", default="tuya",
                        help="Device name filter (default: 'tuya')")
    parser.add_argument("--resize", "-r", type=int, default=0,
                        help="Resize image to NxN before sending (e.g. 466)")
    args = parser.parse_args()

    if not Path(args.image).exists():
        print(f"File not found: {args.image}")
        sys.exit(1)

    asyncio.run(send_image(args.device, args.image, args.resize))


if __name__ == "__main__":
    main()
