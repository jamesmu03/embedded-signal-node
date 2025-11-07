# Embedded Signal Node

A Zephyr RTOS firmware that generates and streams synthetic sensor data over BLE.  
Designed to demonstrate deterministic timing, multi-tasking, and low-power techniques on Nordic nRF52 hardware.

## ⚙️ Hardware
- **MCU:** nRF52833 DK
- **Sensors:** Simulated (no external hardware)

## 🧩 Features
- 8-channel signal generation (1 kHz each)
- BLE UART streaming
- CRC integrity checks
- Watchdog and LED heartbeat
- Power-aware idle mode

## 🚀 Build
```bash
west build -b nrf52833dk_nrf52833
west flash