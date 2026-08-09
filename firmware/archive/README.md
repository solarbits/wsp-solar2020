# Archived sketches

Historical snapshots of the controller firmware, kept for reference. They are
near-identical copies of the same program rather than modules — the live sketch
is `../solar_controller/`, and that is the one to edit.

None of these are wired into the build. To compile one, copy it into a directory
whose name matches the `.ino` basename (Arduino requires this) and point the
Makefile's `SKETCH` at it.

| File | Notes |
| --- | --- |
| `2024-12_plc1_dec2024.ino` | Dec 2024 site build. `MQTT_ID "plc1"`, `minSystemPressure` 20 mb, connects to MQTT synchronously in `setup()`. |
| `2021-01-04_pre-chemistry.ino` | Jan 2021 snapshot. No pH/chlorine readings, no MQTT auth, earlier broker, and several sensor slots still pointed at `ADDR_TEMP_WEST`. |

The differences that matter between these and the live sketch are the MQTT
client ID, the broker address, the low-pressure cut-off, and whether the
chemistry readings exist. Diff against `../solar_controller/solar_controller.ino`
before assuming anything else changed.
