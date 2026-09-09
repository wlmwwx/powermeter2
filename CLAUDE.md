# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
pio run          # Build the project
pio run --target upload   # Build and upload to device
pio run --target upload --upload-port /dev/ttyUSB0  # Upload to specific port
pio run --target clean    # Clean build artifacts
pio run --target test     # Run unit tests
pio run --target menugmon # Open device monitor
```

## Architecture

This is a PlatformIO project for an **ESP32-C3-based power meter** using the Arduino framework. It communicates with an HT7017 energy metering IC via UART.

### Code Structure

```
src/main.cpp     # Main Arduino sketch (setup/loop)
lib/
  HT7017.c/h     # Low-level UART driver for HT7017 metering IC
  Meter.c/h      # Meter abstraction layer - reads U, I, P, Q, S, F, energy
include/         # Project header files (currently unused, imports go in lib/)
test/            # PlatformIO test directory
```

### Key Components

**HT7017 (ATT7053C-compatible) Energy Metering IC**
- Communicates via UART2 at 4800-19200 baud
- Protocol: 6-byte frames with checksum (`0x6A + addr + data + ~sum`)
- Write protection via `0x32` register (0xBC to lock, 0xA6 for calibration range, 0x00 to unlock)
- Register map defined in `HT7017.h` - includes metering registers (0x00-0x1C) and calibration registers (0x30-0x7C)

**Meter Layer**
- `Read_uipq()` - reads real-time values: voltage (0.1V), current (1mA), power (1W), frequency
- `Read_MeterIC_block()` / `Write_MeterIC_block()` - raw register access
- `Init_MeterIC_Reg()` - initializes metering IC with calibration parameters
- `RN8302_AutoCal()` - auto-calibration at known voltage (220V) and current (5A)
- `energy_add()` - accumulates active/reactive energy pulses

**Global Data Structures**
- `MeterCaliPara[]` - calibration parameters (Ugain, I1gain, P1gain, UI1COS, offsets)
- `G_RealTmData[]` - real-time measurements (U, I, P, Q, S, PF, energy)
- `G_DevSts` - device status (voltage, current, power, power factor)

### Hardware Interface

- UART2 (huart2) - HT7017 communication
- UART6 (huart6) - Debug output (prints measurement data)
- The code references `HAL_UART_Transmit_IT` for async debug output
