#!/bin/bash
set -e

# brew update
# brew install arduino-cli

# Setup script for sketch_mar7a_field
# Installs the correct board core and library versions

BOARD_MANAGER_URL="https://apps.industrialshields.com/main/arduino/boards/package_industrialshields_index.json"

echo "Adding Industrial Shields board manager URL..."
bin/arduino-cli config add board_manager.additional_urls "$BOARD_MANAGER_URL"

echo "Installing Industrial Shields AVR core 1.2.1..."
bin/arduino-cli core update-index
bin/arduino-cli core install industrialshields:avr@1.2.1

echo "Installing libraries..."
bin/arduino-cli lib install ArduinoJson@5.13.5
bin/arduino-cli lib install PubSubClient@2.8

# Modbus library from Industrial Shields is not in the Arduino Library Manager,
# so it is vendored in libraries/Modbus (from https://github.com/Industrial-Shields/arduino-Modbus)

echo ""
echo "Done. To compile:"
echo "  bin/arduino-cli compile --fqbn industrialshields:avr:mduino:cpu=mduino19rplus --library libraries/Modbus sketch_mar7a_field.ino"
