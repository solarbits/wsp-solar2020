# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for an Industrial Shields **M-Duino 19R+** PLC that runs the solar pool-heating system at Wotton Pool. Two identical mechanical systems — one panel on the **west** roof face, one on the **east** — share a single controller, so nearly everything in the code is duplicated per side.

Sensors (temperature, pressure) and pump inverters talk **Modbus RTU over RS485**. Motorised valves are driven directly from the PLC's 8 integral relays. Data is published over Ethernet to an **MQTT broker**, consumed by a Node-RED dashboard.

```
firmware/solar_controller/   the live sketch -- edit this one
firmware/libraries/Modbus/   vendored Industrial Shields library
firmware/archive/            historical snapshots, not built
node-red/                    dashboard flow exports
docs/diagrams|photos/        reference PDFs and site photographs
Makefile                     all build and dependency commands
```

`docs/diagrams/` is worth consulting for anything physical: `panels-and-flow-circuit.pdf` (flow circuit), `electrical-diagram_2021-03-07.pdf` (sensor/pump positions and identities), `decision-tree-flow-chart_2020-10-20.pdf` (the `decisionTree()` state logic), `software-architecture_2021-04-28_with-chemistry.pdf`.

## Build and upload

All commands run **from the repository root**.

```bash
make setup                    # install industrialshields:avr core + ArduinoJson/PubSubClient
make build                    # compile into build/
make upload                   # compile + upload
make upload PORT=/dev/ttyUSB0 # override serial port (default /dev/cu.usbmodem2301)
make monitor                  # serial monitor at 115200
make ports                    # list attached boards
make clean
```

`CLI` resolves to `bin/arduino-cli` if that exists, else `arduino-cli` from `PATH`.

**Library resolution is fragile and the Makefile compensates for it.** Unrelated third-party libraries publish the names `Modbus`, `ModbusRTUMaster` and `RS485`, and a copy in the user's global `~/Documents/Arduino/libraries` will otherwise win. The failure is confusing rather than obvious — e.g. Rob Tillaart's `RS485` declares a *class* named `RS485`, so `ModbusRTUMaster master(RS485);` parses as a function declaration and every later `master.foo()` errors with "request for member ... in 'master', which is of non-class type". The fix already in place is `--library` (singular, top priority) for both the vendored Modbus library and the board core's RS485; `--libraries` (plural, just a search path) is **not** sufficient. If you see that error class, check the "Used library" table in the compile output before touching the source.

`sketch.yaml` records the pinned versions but **cannot build**: arduino-cli profiles can't reference vendored libraries. Keep it in sync with the Makefile's version variables.

There are no tests and no linter. Verification is compile + upload + serial monitor on real hardware. The **Ethernet interface only comes up under external PSU power** — USB power is enough to flash but not to run.

## Sketch versions

`firmware/archive/` holds near-identical copies of the same program — snapshots, not modules. Make changes in `firmware/solar_controller/` unless explicitly asked otherwise, and don't try to factor the copies together. Arduino requires the `.ino` basename to match its directory name, so renaming the sketch means renaming the directory too.

## Architecture of the sketch

Single file, ~650 lines, flat C-style functions. Everything is coordinated through module-level arrays indexed by `INDEX_*` constants, with parallel `ADDR_*` arrays holding the Modbus slave addresses. To add a sensor you must update `numberOfSensors`, `sensorAddresses[]`, `sensorReadings[]`, and add matching `ADDR_`/`INDEX_` defines — and check `requestReadFrom()`, which branches on `sensorIndex >= 6` to decide temperature vs. pressure register layout (so **ordering within the array is load-bearing**).

`loop()` never blocks. Two independent millis()-based timers:

- **~80 ms tick** — issues exactly one Modbus transaction, then services MQTT. A `modbusRequestMode` state machine walks the whole sensor array in `MODBUS_REQUEST_READ`, flips to `MODBUS_REQUEST_WRITE` to push all pump states, then flips back. `listenForResponse()` is called every iteration (not on the tick) and dispatches replies by slave address; readings are matched back into `sensorReadings[]` by searching `sensorAddresses[]`.
- **5 s tick** — runs `decisionTree()`, `readChemistry()`, and publishes everything.

Pressure transducers return a 32-bit float split across two input registers in a swapped word order; `convert32BitFloatToInt()` reassembles it byte-by-byte and scales to millibars. `handleResponseData()` silently drops readings outside −500…3000 as spurious.

### Units (integers everywhere — no floats in stored state)

| Quantity | Encoding |
| --- | --- |
| Temperature | tenths of °C (`300` = 30 °C); the `+5` in `decisionTree()` is a 0.5 °C threshold |
| Pressure | millibars (float × 1000) |
| Power | **decawatts** — kept small so the JSON int stays under 32767; `ΔT × 14` from 20 L/min flow |
| pH | hundredths (`700` = pH 7.0) |
| Chlorine | hundredths of ppm |

### `decisionTree()`

Runs every 5 s and is the whole control policy. It sets `pumpStates[]` (pushed to the inverters on the next Modbus write cycle) and drives the relays immediately, and records one of five `roofStates[]` per side: `LOW_SYST_PRES`, `POOL_TOO_HOT`, `COOL_ROOF`, `HEAT_POOL`, `AWAIT_SUNSHINE`. Low suction pressure is checked first and short-circuits both sides off — this is the dry-run protection that stops the pumps destroying themselves, so preserve that early return. West and east are then evaluated by duplicated blocks; changes to one almost always need mirroring in the other.

### Relay/valve polarity

The four relay helpers (`switchWestOff`, `coolWestRoof`, `heatPoolFromWest`, and east equivalents) are the only place relays are touched. Flow/return valves are **active-high** (`HIGH` = open) but the **drain valves are inverted** (`HIGH` = closed, `LOW` = open). Read the comment on each `digitalWrite` rather than inferring from the level. Relay-to-valve mapping is `R0_1`–`R0_8`, east on `R0_1`–`R0_4` and west on `R0_5`–`R0_8`.

### MQTT

Every value is published individually to topic `IP65` as `{"zone":N,"index":I,"value":V}` via ArduinoJson 5 (`DynamicJsonBuffer` — v5 API, not v6/v7). Zones: `0` sensors, `1` pumps, `2` roof states, `3` roof powers, `4` chemistry. `index` is the corresponding `INDEX_*` value. `node-red/README.md` documents the full contract; changing zone/index numbering or the topic breaks the dashboard flows in `node-red/`.

Broker IP, port, `MQTT_ID`, `MQTT_USER`/`MQTT_PASS` are `#define`d at the top of the sketch; past brokers are left commented out. **`MQTT_ID` must be unique per physical PLC** — two boards sharing a client ID will repeatedly kick each other off the broker.
