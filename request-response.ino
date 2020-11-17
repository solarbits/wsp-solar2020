/*
  Exploring how to handle responses when they arrive,
  and to request data from multiple devices
*/

#include <ModbusRTUMaster.h>
#include <RS485.h>

// Device addresses, (for example)
#define ADDR_TEMP_WEST 33
#define ADDR_TEMP_EAST 23

#define ADDR_PUMP_WEST 2
#define ADDR_PUMP_EAST 2

// Device states
#define DRIVE_PUMP_ON 1
#define DRIVE_PUMP_OFF 0

const int numberOfSensors = 2; // Update this if you add another one below
const int addresses[] = {ADDR_TEMP_WEST, ADDR_TEMP_EAST};
// Current device being read (index in the above array)
int deviceIndex = 0;

ModbusRTUMaster master(RS485);

// Timestamps to throttle infrequent operations
uint32_t lastSentTime = 0UL;
uint32_t lastPrintedTime = 0UL;

// Config
const uint32_t BAUDRATE = 9600UL;

// Save readings globally here
int tempWest;
int tempEast;
// ... and others

void setup() {
  Serial.begin(BAUDRATE);
  RS485.begin(BAUDRATE, HALFDUPLEX, SERIAL_8N1);
  master.begin(BAUDRATE);
}

void loop() {
  // Check that its been a second since last request for data
  // This is like delay(1000) but doesn't block the loop
  // The loop should keep looping incase a response comes along
  if (millis() - lastSentTime > 1000) {

    // Infrequently request data
    // Each time this is called, it will move to the next address in the addresses array
    requestNextData();

    lastSentTime = millis();
  }

  // Check to see if a response is waiting every loop
  // This function will print the result if anything comes
  listenForResponse();

  // Same again here. Periodically, and unblockingly print out whatever is saved globally
  // This will be what is the state of the saved vars on which we decide whether to run
  // the pumps or not. This happens every 10 seconds
  if (millis() - lastPrintedTime > 10000) {

    Serial.print("Saved values: [West]: ");
    Serial.print(tempWest);
    Serial.print(" [East]: ");
    Serial.println(tempEast);

    // Using the saved values, decide what to do
    decisionTree();

    lastPrintedTime = millis();
  }
}

// Replace this bit with the decision tree
void decisionTree() {
  if (tempWest > tempEast) {
    headPoolFromWest();
  } else {
    coolPanelWest();
  }
}

void headPoolFromWest() {
  Serial.println("Heat pool from west");
  drivePumpFor(ADDR_PUMP_WEST, DRIVE_PUMP_ON);
  // ... Relays etc
}

void coolPanelWest() {
  Serial.println("Cool panel west");
  drivePumpFor(ADDR_PUMP_WEST, DRIVE_PUMP_OFF);
  // ... Relays etc
}

void requestNextData() {
  // Look up the Modbus address for this index
  int deviceAddress = addresses[deviceIndex];

  // Send request to the sensor
  requestDataFrom(deviceAddress);

  // Increment the device index, to lookup the next sensor address next time
  // Or go back to the start
  if (deviceIndex == numberOfSensors -1) {
    deviceIndex = 0;
  } else {
    deviceIndex = deviceIndex + 1;
  }
}

// Request data infrequently
// passing the address of the device to send the request to
void requestDataFrom(int deviceAddress) {
  Serial.print("Requesting data from: addr-");
  Serial.println(deviceAddress);

  if (!master.readInputRegisters(deviceAddress, 1, 1)) {
    Serial.print("Trouble requesting data from device: addr-");
    Serial.println(deviceAddress);
  }
}

// Set power state for a pump by address
void drivePumpFor(int deviceAddress, int value) {
  if (!master.writeSingleRegister(deviceAddress, 0, value)) {
    Serial.print("Trouble sending power command to device: addr-");
    Serial.println(deviceAddress);
  }
}

// Check often if a response is waiting
// If there isn't this will just do nothing and the loop continues
// until something happens
void listenForResponse () {
  if (master.isWaitingResponse()) {
    ModbusResponse response = master.available();

    if (response) {
      // Find out from which device this response is coming so we know what to do with it
      int deviceAddress = response.getSlave();

      if (response.hasError()) {
        // Debug message about the error
        Serial.print("Error response from device: addr-");
        Serial.print(deviceAddress);
        Serial.print(" Error code: ");
        Serial.println(response.getErrorCode());

      } else {
        // Get the coil value from the response
        int reading = response.getRegister(0);

        // So now we have the reading, and the address that it came from
        // so it can be dealt with, eg. Save it into the right variable
        handleResponseData(deviceAddress, reading);
      }
    }
  }
}

// Response packet comes back with the address and the reading.
// This function will print it for debugging, and saves it to the tempEast and tempWest vars
void handleResponseData (int deviceAddress, int reading) {
  Serial.print("Response from: addr-");
  Serial.print(deviceAddress);

  Serial.print(" Reading: ");
  Serial.println(reading);

  // Set the global var from the reading, based on the address that it came from
  switch (deviceAddress) {
    case ADDR_TEMP_WEST: tempWest = reading;
      break;
    case ADDR_TEMP_EAST: tempEast = reading;
      break;
    default:
      break;
  }
}
