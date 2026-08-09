# wsp-solar2020

Code and engineering details for the solar heating system at Wotton Pool. The heating warms the pool water using two panels: one on the west face of a pitched roof and the other on the east. By good fortune (not design!) the ridge of the roof runs exactly north–south. There are two identical mechanical systems, so the single control system uses 'west' or 'east' to identify the relevant valves, pumps etc.

The code generates firmware for an M-Duino 19R+ programmable logic controller, written to simplify coding for a less code-aware audience/maintenance. It is C++ and uses a number of simplifying utility functions provided by the PLC supplier (Boot and Work, Barcelona).

The sensors are all digital and communicate using Modbus RTU. Similarly the pumps are activated through inverters using Modbus RTU. There are mains-operated motorised valves that are driven directly through 8 integral relays on the PLC module.

The decisions whether/how to operate the panel, which involves instructing the pumps and the valves, are made in the C++ function `decisionTree()`. It puts each roof into one of five states. One state switches everything off if the pressure on the suction side of the solar pumps is low. This inhibits the operation of the pumps when there is (for any reason) no water available on their inputs and the pumps would otherwise run dry and damage themselves. The other four states are self-explanatory.

The PLC also broadcasts the sensor data, the on/off state of the pumps and the operational status of the roof. It does this through an Ethernet connection to an MQTT broker which is subscribed to by Node-RED, which builds a data dashboard for remote monitoring.

## Layout

```
firmware/
  solar_controller/     the live sketch -- edit this one
  libraries/Modbus/     vendored Industrial Shields Modbus library
  archive/              historical sketch snapshots, not built
node-red/               Node-RED flow exports for the dashboard
docs/
  diagrams/             circuit, electrical and software architecture PDFs
  photos/               site photographs
Makefile                build, upload and dependency management
```

## Documentation

| Document | What it shows |
| --- | --- |
| `docs/diagrams/panels-and-flow-circuit.pdf` | The flow circuit, the panels and the pump layout |
| `docs/diagrams/electrical-diagram_2021-03-07.pdf` | C&I electrical layout — positions and identity of pumps and sensors (`electrical-diagram_2019-08-08.pdf` is the earlier revision) |
| `docs/diagrams/decision-tree-flow-chart_2020-10-20.pdf` | The logic implemented by `decisionTree()` |
| `docs/diagrams/software-architecture_2021-04-28_with-chemistry.pdf` | How the code operates (`software-architecture_2020-12-31.pdf` predates the pH/chlorine readings) |
| `docs/photos/` | General layout of the main parts |

## Building

### Prerequisites

[arduino-cli](https://arduino.github.io/arduino-cli/) on your `PATH` (`brew install arduino-cli`), or a binary placed at `bin/arduino-cli` — the Makefile prefers the local one if it exists.

### Commands

Run from the repository root:

```bash
make setup                     # install the board core and pinned libraries
make build                     # compile
make upload                    # compile and upload
make upload PORT=/dev/ttyUSB0  # upload to a different port
make monitor                   # serial monitor at 115200 baud
make ports                     # list attached boards and their serial ports
make clean                     # remove build artifacts
make help                      # list targets
```

The default serial port is `/dev/cu.usbmodem2301`; use `make ports` to find yours.

### Dependencies

| Dependency | Version | Source |
| --- | --- | --- |
| industrialshields:avr (board core) | 1.2.1 | [Industrial Shields board manager](https://apps.industrialshields.com/main/arduino/boards/package_industrialshields_index.json) |
| ArduinoJson | 5.13.5 | Arduino Library Manager |
| PubSubClient | 2.8 | Arduino Library Manager |
| Modbus (Industrial Shields) | 1.1.0 | Vendored in `firmware/libraries/Modbus` ([GitHub](https://github.com/Industrial-Shields/arduino-Modbus)) |

The Modbus library is vendored because it is not available in the Arduino Library Manager. The Makefile passes it — and the board core's `RS485` library — with `--library`, which forces them to take priority over anything of the same name in your global `~/Documents/Arduino/libraries` folder. Several unrelated third-party libraries publish the names `Modbus`, `ModbusRTUMaster` and `RS485`, and if one of those wins the resolution race the sketch will not compile.

### Running on the bench

The Ethernet board only works if the PLC is powered from an external power supply. You can flash the board over USB power, but use the power supply to run it.

## MQTT

The sketch publishes to topic `IP65` as `{"zone":N,"index":I,"value":V}`. Broker address, port and credentials are `#define`d at the top of `firmware/solar_controller/solar_controller.ino`. `MQTT_ID` must be unique per physical PLC.
