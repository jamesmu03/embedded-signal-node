import asyncio
import sys
import time
from bleak import BleakScanner, BleakClient

# Nordic UART Service UUIDs
NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
NUS_TX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc

def notification_handler(sender, data):
    try:
        # Decode data
        text = data.decode('utf-8').strip()
        parts = text.split(',')
        
        if len(parts) < 2:
            return

        # Extract CRC (last field)
        received_crc_hex = parts[-1]
        payload = text[:-(len(received_crc_hex) + 1)] # Remove ",CRC"
        
        try:
            received_crc = int(received_crc_hex, 16)
        except ValueError:
            print(f"Invalid CRC format: {text}")
            return

        # Calculate CRC
        calculated_crc = crc16_ccitt(payload.encode('utf-8'))
        
        if received_crc != calculated_crc:
            print(f"CRC Mismatch! Recv: {received_crc:04X}, Calc: {calculated_crc:04X}")
            return

        # Parse fields
        timestamp = int(parts[0])
        channels = [int(x) for x in parts[1:-1]]
        
        print(f"TS: {timestamp} | Ch: {channels}")
        
    except Exception as e:
        print(f"Error parsing packet: {e}")

async def main():
    print("Scanning for 'BioSignal'...")
    device = await BleakScanner.find_device_by_name("BioSignal")
    
    if not device:
        print("Device 'BioSignal' not found.")
        return

    print(f"Found {device.name} ({device.address}). Connecting...")
    
    async with BleakClient(device) as client:
        print(f"Connected: {client.is_connected}")
        
        await client.start_notify(NUS_TX_CHAR_UUID, notification_handler)
        
        print("Receiving data for 2 minutes... (Press Ctrl+C to stop early)")
        start_time = time.time()
        packet_count = 0
        expected_packets = 0
        last_ts = None
        
        try:
            while time.time() - start_time < 120: # Run for 2 minutes
                await asyncio.sleep(1)
        except asyncio.CancelledError:
            pass
        finally:
            await client.stop_notify(NUS_TX_CHAR_UUID)
            print("\n--- Test Complete ---")
            # Note: This is a simple estimation. For precise loss, we'd need a sequence number in the packet.
            # Assuming 100Hz (10ms period).
            print("Please check the console output for any CRC errors.")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nUser stopped script.")
