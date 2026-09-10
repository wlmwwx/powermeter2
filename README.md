# Powermeter2 — Dual-Channel Smart Power Meter

ESP32-C3 + HT7017 dual-channel AC power meter with WiFi provisioning, MQTT reporting, and local LCD + 5-way keypad control.

See `docs/PRD.md` for product requirements and `docs/superpowers/plans/` for the implementation plan.

## Features

- Dual-channel AC measurement (voltage + 2× current via CTs)
- WiFi SoftAP + Web provisioning (no app required)
- MQTT periodic state publishing with availability LWT
- MQTT command subscription for relay control (2 channels)
- Persistent calibration (NVS) and energy snapshot (LittleFS)
- ST7735 128x160 SPI LCD for live metering display
- 5-way navigation keypad with on-screen menu (relay toggle, status, reset)
- OK + UP hold (>5s) for factory reset

## Build

```bash
pio run                       # Build
pio run --target upload       # Build + upload to default serial port
pio run --target upload --upload-port /dev/ttyUSB0   # Specific port
pio run --target menugmon     # Open device monitor
```

## First boot

1. Power the device from 220V AC.
2. ESP32-C3 boots into SoftAP mode (SSID `Powermeter_XXXXXX` — last 6 MAC hex digits).
3. Connect to the AP. Default password: `12345678`.
4. Open <http://192.168.4.1>.
5. Submit WiFi credentials and MQTT broker config.
6. Device reboots into STA, connects WiFi, then MQTT.

## MQTT topics

| Direction | Topic | Payload |
|-----------|-------|---------|
| Publish | `{prefix}/{device_id}/state` | JSON, every 5s (configurable 1-60s) |
| Publish | `{prefix}/{device_id}/availability` | `online` / `offline` (LWT, retained) |
| Subscribe | `{prefix}/{device_id}/cmd` | JSON: `{"ch1": bool, "ch2": bool}` |

Default `prefix` is `powermeter`. Device ID is auto-generated from MAC.

## State payload (abbreviated)

```json
{
  "device_id": "A1B2C3",
  "ts": 1700000000,
  "wifi": { "rssi": -55 },
  "ch1": { "u": 220.1, "i": 0.150, "p": 33.0, "q": 5.0, "s": 33.4, "pf": 0.987, "ep": 1.23, "eq": 0.45 },
  "ch2": { "u": 220.1, "i": 0.000, "p": 0.0,  "q": 0.0, "s": 0.0,  "pf": 1.000, "ep": 0.00, "eq": 0.00 },
  "f": 50.00,
  "relay": { "ch1": true, "ch2": false }
}
```

## Display & Controls

The 128x160 ST7735 LCD shows live metering. The 5-way keypad navigates the on-screen menu.

**Home screen:** U, I1/I2, P1/P2, EP1/EP2, F, CH1/CH2 relay state, "OK=menu" hint at bottom.

**Menu items (in order):** WiFi status / MQTT status / Channel 1 toggle / Channel 2 toggle / Backlight toggle / Reset

**Keypad actions:** UP/DOWN scroll selection, OK select, LEFT back to home.

**Factory reset:** from any screen, hold OK + UP for >5 seconds (device reboots into SoftAP).

## Pin map

| Pin | Function |
|-----|----------|
| GPIO 2 | LCD SCLK |
| GPIO 3 | LCD MOSI |
| GPIO 4 | Nav: OK |
| GPIO 5 | Nav: LEFT |
| GPIO 6 | LCD DC |
| GPIO 7 | LCD CS |
| GPIO 8 | Nav: UP |
| GPIO 9 | Nav: RIGHT |
| GPIO 10 | LCD RST |
| GPIO 11 | LCD Backlight |
| GPIO 12 | Relay 1 (HIGH = on) |
| GPIO 13 | Nav: DOWN |
| GPIO 18 | Relay 2 (HIGH = on, USB-CDC warning) |
| GPIO 20 | HT7017 UART RX (ESP32-C3 RX) |
| GPIO 21 | HT7017 UART TX |

HT7017 UART: 4800 baud, 8E1.

## Repository layout

```
docs/PRD.md                  Product Requirements
docs/superpowers/plans/      Implementation plan + ledger
lib/HT7017/                  UART driver for HT7017 metering IC
lib/Meter/                   Calibration + dual-channel math + energy accum
lib/Config/                  NVS-backed settings (wifi/mqtt/device/cali)
lib/EnergyStore/             LittleFS snapshot of cumulative energy
lib/Provisioning/            SoftAP + Web provisioning HTTP server
lib/MqttClient/              PubSubClient wrapper (state/availability/cmd)
lib/Display/                 ST7735 LCD wrapper
lib/NavKey/                  5-way debounced nav + OK+UP combo
src/main.cpp                 Orchestrator: setup() + loop()
platformio.ini               PlatformIO + Arduino for ESP32-C3
```

## License

TBD
