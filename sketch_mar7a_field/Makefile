FQBN = industrialshields:avr:mduino:cpu=mduino19rplus
PORT ?= /dev/ttyACM0
SKETCH = sketch_mar7a_field.ino
CLI = bin/arduino-cli
BOARD_MANAGER_URL = https://apps.industrialshields.com/main/arduino/boards/package_industrialshields_index.json

.PHONY: setup build upload monitor clean

setup:
	$(CLI) config add board_manager.additional_urls "$(BOARD_MANAGER_URL)"
	$(CLI) core update-index
	$(CLI) core install industrialshields:avr@1.2.1
	$(CLI) lib install ArduinoJson@5.13.5
	$(CLI) lib install PubSubClient@2.8

build:
	$(CLI) compile --fqbn $(FQBN) --library libraries/Modbus $(SKETCH)

upload: build
	$(CLI) upload --fqbn $(FQBN) -p $(PORT) $(SKETCH)

monitor:
	$(CLI) monitor -p $(PORT) --config baudrate=115200

clean:
	rm -rf build/
