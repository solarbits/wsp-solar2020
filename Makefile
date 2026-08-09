# Build/upload firmware for the Wotton Pool solar heating PLC.
# Run from the repository root:  make setup && make build && make upload

FQBN      = industrialshields:avr:mduino:cpu=mduino19rplus
PORT     ?= /dev/cu.usbmodem2301
BAUD     ?= 115200

SKETCH    = firmware/solar_controller
LIB_DIR   = firmware/libraries
BUILD_DIR = build

# --library (singular) gives these top priority over anything in the user's
# global ~/Documents/Arduino/libraries folder. This matters: unrelated libraries
# publish the names "Modbus"/"ModbusRTUMaster" and "RS485", and if a global one
# wins the resolution race the sketch fails to compile (e.g. Rob Tillaart's
# RS485 declares a *class* named RS485, so `ModbusRTUMaster master(RS485);`
# parses as a function declaration). RS485 ships inside the board core, so its
# path is derived from arduino-cli's data directory rather than hardcoded.
ARDUINO_DATA_DIR = $(shell $(CLI) config get directories.data 2>/dev/null)
CORE_LIB_DIR     = $(ARDUINO_DATA_DIR)/packages/industrialshields/hardware/avr/$(CORE_VER)/libraries

VENDORED_LIBS = --library $(LIB_DIR)/Modbus --library $(CORE_LIB_DIR)/RS485

# Prefer a project-local bin/arduino-cli if one has been dropped in,
# otherwise fall back to whatever is on PATH (e.g. `brew install arduino-cli`).
CLI ?= $(shell [ -x bin/arduino-cli ] && echo bin/arduino-cli || echo arduino-cli)

BOARD_MANAGER_URL = https://apps.industrialshields.com/main/arduino/boards/package_industrialshields_index.json

# Pinned dependency versions. Keep in sync with firmware/solar_controller/sketch.yaml.
CORE_VER             = 1.2.1
CORE_VERSION         = industrialshields:avr@$(CORE_VER)
ARDUINOJSON_VERSION  = ArduinoJson@5.13.5
PUBSUBCLIENT_VERSION = PubSubClient@2.8

.PHONY: help setup build upload monitor ports clean check

help: ## Show this help
	@grep -hE '^[a-z-]+:.*?## ' $(MAKEFILE_LIST) | awk -F':.*?## ' '{printf "  \033[36m%-10s\033[0m %s\n", $$1, $$2}'

check: ## Verify arduino-cli is available
	@command -v $(CLI) >/dev/null 2>&1 || { \
		echo "arduino-cli not found."; \
		echo "Install it (brew install arduino-cli) or place a binary at bin/arduino-cli."; \
		exit 1; }
	@$(CLI) version

setup: check ## Install the board core and pinned libraries
	$(CLI) config add board_manager.additional_urls "$(BOARD_MANAGER_URL)"
	$(CLI) core update-index
	$(CLI) core install $(CORE_VERSION)
	$(CLI) lib install $(ARDUINOJSON_VERSION)
	$(CLI) lib install $(PUBSUBCLIENT_VERSION)
	@echo "Modbus 1.1.0 is vendored in $(LIB_DIR)/Modbus -- nothing to install."

build: check ## Compile the sketch
	$(CLI) compile --fqbn $(FQBN) $(VENDORED_LIBS) --build-path $(BUILD_DIR) $(SKETCH)

upload: build ## Compile and upload to the PLC (override with PORT=...)
	$(CLI) upload --fqbn $(FQBN) -p $(PORT) --input-dir $(BUILD_DIR) $(SKETCH)

monitor: check ## Open the serial monitor
	$(CLI) monitor -p $(PORT) --config baudrate=$(BAUD)

ports: check ## List attached boards and their serial ports
	$(CLI) board list

clean: ## Remove build artifacts
	rm -rf $(BUILD_DIR)
