# sketch_mar7a_field

Arduino sketch for an Industrial Shields M-Duino 19R+ PLC that controls a solar pool heating system.

Uses Modbus RTU over RS485 to read temperature and pressure sensors and control pumps/valves, and publishes data via MQTT.

## Prerequisites

- [arduino-cli](https://arduino.github.io/arduino-cli/) installed (e.g. `brew install arduino-cli`)

## Setup

Install the board core and libraries:

```bash
make setup
```

This installs:

| Dependency | Version | Source |
|---|---|---|
| industrialshields:avr (board core) | 1.2.1 | [Industrial Shields board manager](https://apps.industrialshields.com/main/arduino/boards/package_industrialshields_index.json) |
| ArduinoJson | 5.13.5 | Arduino Library Manager |
| PubSubClient | 2.8 | Arduino Library Manager |
| Modbus (Industrial Shields) | 1.1.0 | Vendored in `libraries/Modbus` ([GitHub](https://github.com/Industrial-Shields/arduino-Modbus)) |

The Modbus library is vendored in the repo because it is not available in the Arduino Library Manager.

## Build and Upload

A Makefile is provided for convenience. The default serial port is `/dev/ttyACM0`.

```bash
make setup                    # install board core and libraries
make build                    # compile the sketch
make upload                   # compile and upload
make upload PORT=/dev/ttyUSB0 # upload to a different port
make monitor                  # open serial monitor (115200 baud)
make clean                    # remove build artifacts
```

To find the serial port for your board:

```bash
bin/arduino-cli board list
```

## MQTT

The sketch publishes sensor data to an MQTT broker on topic `IP65`. The broker address is configured in the sketch as an `IPAddress`.
