# Powermeter2 — Dual-Channel Smart Power Meter

ESP32-C3 + HT7017 dual-channel AC power meter with WiFi provisioning and MQTT reporting.

See `docs/PRD.md` for product requirements and `docs/superpowers/plans/` for the implementation plan.

## Features

- Dual-channel AC measurement (voltage + 2× current via CTs)
- WiFi SoftAP + Web provisioning (no app required)
- MQTT periodic state publishing with availability LWT
- MQTT command subscription for relay control (2 channels)
- Persistent calibration (NVS) and energy snapshot (LittleFS)
- Long-press key (>5s) for factory reset

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

## Pin map

| Pin | Function |
|-----|----------|
| GPIO 20 | HT7017 UART RX (ESP32-C3 RX) |
| GPIO 21 | HT7017 UART TX |
| GPIO 8 | Status LED |
| GPIO 2 | Relay 1 |
| GPIO 3 | Relay 2 |
| GPIO 9 | Key (input pull-up) |

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
src/main.cpp                 Orchestrator: setup() + loop()
platformio.ini               PlatformIO + Arduino for ESP32-C3
```

## License

TBD
