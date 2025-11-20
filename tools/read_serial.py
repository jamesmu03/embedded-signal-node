#!/usr/bin/env python3
"""
Simple script to read UART output from the nRF52833 board.
Shows the 8-channel simulated electrode data.
"""

import serial
import sys
import time

PORT = '/dev/tty.usbmodem0010506285681'
BAUDRATE = 115200

try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    print(f"Connected to {PORT} at {BAUDRATE} baud")
    print("Reading 8-channel data... Press Ctrl+C to stop\n")
    
    # Read and display lines
    line_count = 0
    while line_count < 50:  # Read 50 lines for verification
        line = ser.readline()
        if line:
            try:
                decoded = line.decode('utf-8').strip()
                if decoded:
                    print(decoded)
                    line_count += 1
            except UnicodeDecodeError:
                pass
    
    ser.close()
    print(f"\n✓ Successfully read {line_count} lines of 8-channel data!")
    
except serial.SerialException as e:
    print(f"Error: Could not open {PORT}")
    print(f"Details: {e}")
    sys.exit(1)
except KeyboardInterrupt:
    print("\nStopped by user")
    ser.close()
    sys.exit(0)
