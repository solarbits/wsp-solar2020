
/*
  Exploring how to handle responses when they arrive,
  and to request data from multiple devices
*/

#include <ModbusRTUMaster.h>
#include <RS485.h>

// #define DEBUG_REQUESTS 1 // Uncomment to enable
#define DEBUG_RESPONSES 1 // Comment to disable
#define DEBUG_ERRORS 1 // Comment to disable

// Device addresses, (for example)
#define ADDR_TEMP_WEST      23
#define ADDR_TEMP_WEST_PIPE 24
#define ADDR_TEMP_EAST      33
#define ADDR_TEMP_EAST_PIPE 34
#define ADDR_TEMP_POOL      21
#define ADDR_TEMP_AIR       31

#define ADDR_PRES_WEST      27
#define ADDR_PRES_EAST      37
#define ADDR_PRES_SYST      64

// Indexes to look these up in the state array
#define INDEX_TEMP_WEST 0
#define INDEX_TEMP_WEST_PIPE 1
#define INDEX_TEMP_EAST 2
#define INDEX_TEMP_EAST_PIPE 3
#define INDEX_TEMP_POOL 4
#define INDEX_TEMP_AIR 5
#define INDEX_PRESS_WEST 6
#define INDEX_PRESS_EAST 7
#define INDEX_PRESS_SYSTEM 8

#define ADDR_PUMP_WEST 2
#define ADDR_PUMP_EAST 2 //CHANGE THIS TO 1 LATER
// Indexes to look these up in the state array
#define INDEX_PUMP_WEST 0
#define INDEX_PUMP_EAST 1

// Device states
#define DRIVE_PUMP_ON 1
#define DRIVE_PUMP_OFF 0

const int numberOfSensors = 2; // Update this if you add another one below
const int sensorAddresses[] = {ADDR_TEMP_WEST, ADDR_TEMP_EAST};
const int numberOfPumps = 2;
const int pumpAddresses[] = {ADDR_PUMP_WEST, ADDR_PUMP_EAST};

// Current device being read (index in the above array)
int sensorIndex = 0;
int pumpIndex = 0;

// Written into here by decision tree, then looked up by index sequentially by the modbus send loop
int pumpStates[] = {DRIVE_PUMP_OFF, DRIVE_PUMP_OFF};
// Written into by recieving responses from MB sensors
int sensorReadings[8] = {};

// Modbus modes, when all addresses from each mode have been read or written, then move onto the next mode
#define MODBUS_REQUEST_READ 1
#define MODBUS_REQUEST_WRITE 0
int modbusRequestMode = MODBUS_REQUEST_READ;

ModbusRTUMaster master(RS485);

// Timestamps to throttle infrequent operations
uint32_t lastSentTime = 0UL;
uint32_t lastPrintedTime = 0UL;

// Config
const uint32_t BAUDRATE = 9600UL;

// define limiting parameters here
const int maxPoolTemp = 300; // 30degC
const int maxPanelTemp = 600; //(60degC)
const int minSystemPressure = 35; // (35mb)

void setup() {
  Serial.begin(BAUDRATE);
  RS485.begin(BAUDRATE, HALFDUPLEX, SERIAL_8N1);
  master.begin(BAUDRATE);
}

void loop() {
  // Check that its been a second since last request for data
  // This is like delay(1000) but doesn't block the loop
  // The loop should keep looping incase a response comes along
  if (millis() - lastSentTime > 50) {

    // Infrequently request data
    // Each time these request functions are called, it will move to the next address in the addresses array until switching mode when the array is complete
    switch modbusRequestMode {
      case MODBUS_REQUEST_READ:
        requestNextRead()
        break;
      case MODBUS_REQUEST_WRITE:
        requestNextWrite()
        break;
    }

    lastSentTime = millis();
  }

  // Check to see if a response is waiting every loop
  // This function will print the result if anything comes
  listenForResponse();

  // Same again here. Periodically, and unblockingly print out whatever is saved globally
  // This will be what is the state of the saved vars on which we decide whether to run
  // the pumps or not. This happens every 10 seconds
  if (millis() - lastPrintedTime > 10000) {

    Serial.print("Saved values: ");
    for (int i = 0; i < numberOfSensors; i++) {
      Serial.print(sensorReadings[i]);
      Serial.print(", ");
    }
    Serial.println();

    // Using the saved values, decide what to do
    // and set new pump states based on temperature comparisons
    decisionTree();

    lastPrintedTime = millis();
  }
}

// Using the saved values, decide what to do
// and set new pump states and set relays
// based on temperature comparisons
void decisionTree() {
  // once we've got data check:
  // the line pressure is ok,
  // Turn off the pumps and bail out if not

  // Otherwise check
  // whether the pool is too hot
  // then whether the west panel is too hot
  // then whether the west panel is hot enough to heat the pool
  // then again whether the panel is too hot
  // then whether the east panel is too hot
  // then whether the east panel is hot enough to heat the pool

  if (sensorReadings[INDEX_PRESS_SYSTEM] < minSystemPressure) {

    // the line pressure is too low: switch the pumps off
    switchWestOff();
    switchEastOff();
    return;
  }

  // the line pressure is ok, carry on :-)

  if (sensorReadings[INDEX_TEMP_POOL] > maxPoolTemp) {

    //  Pool is hot enough, check whether west panel is getting too hot
    if (sensorReadings[INDEX_TEMP_WEST_PIPE] > maxPanelTemp) {

      // Whoops, west panel is getting too hot: cool the roof
      coolWestRoof();

    } else {

      // No, the panel is cool enough, stop pumping water through it to waste
      switchWestOff();
    }

  } else {

    // No the pool isn't too hot, is the west panel hotter than the pool?
    if (sensorReadings[INDEX_TEMP_WEST_PIPE] > poolTemp) {

      // Yes, its hotter than the pool and the pool needs the heat
      heatPoolFromWest();

    } else {

      // No, the west panel is too cold
      switchWestOff();
    }
  }




  // that deals with the west side, now look at east. Repeat the checks every so often...

  if (sensorReadings[INDEX_TEMP_POOL] > maxPoolTemp) {

    //  Pool is hot enough, check whether east panel is getting too hot
    if (sensorReadings[INDEX_TEMP_EAST_PIPE] > maxPanelTemp) {

      // Whoops, east panel is getting too hot, cool the roof
      coolEastRoof();

    } else {

      // No, the panel is cool enough, stop pumping water through it to waste
      switchEastOff();
    }

  } else {

    // No the pool isn't too hot, is the east panel hotter than the pool?
    if (sensorReadings[INDEX_TEMP_EAST_PIPE] > poolTemp) {

      // Yes, it's hotter than the pool and the pool needs the heat
      heatPoolFromEast();

    } else {

      // No, the panel is too cold so switch it off
      switchEastOff();
    }
  }
}

void switchWestOff() {
  Serial.println("Switch west off");
  pumpStates[INDEX_PUMP_WEST] = DRIVE_PUMP_OFF;
  // ... Relays etc
}
void switchEastOff() {
  Serial.println("Switch east off");
  pumpStates[INDEX_PUMP_EAST] = DRIVE_PUMP_OFF;
  // ... Relays etc
}
void coolWestRoof() {
  Serial.println("Cool panel west");
  pumpStates[INDEX_PUMP_WEST] = DRIVE_PUMP_ON;
  // ... Relays etc
}
void coolEastRoof() {
  Serial.println("Cool panel east");
  pumpStates[INDEX_PUMP_EAST] = DRIVE_PUMP_ON;
  // ... Relays etc

}
void heatPoolFromWest() {
  Serial.println("Heat pool from west");
  pumpStates[INDEX_PUMP_WEST] = DRIVE_PUMP_ON;
  // ... Relays etc
}
void heatPoolFromEast() {
  Serial.println("Heat pool from east");
  pumpStates[INDEX_PUMP_EAST] = DRIVE_PUMP_ON;
  // ... Relays etc
}

// Send requests to the sensors one by one
// until they're all done, then switch the mode to write to send commands to the pumps
void requestNextRead() {
  // Look up the Modbus address for this index
  int deviceAddress = sensorAddresses[sensorIndex];

  // Send a read register command to the sensor
  requestReadFrom(deviceAddress);

  // Increment the device index, to lookup the next sensor address next time
  // Or go back to the start
  if (sensorIndex == numberOfSensors - 1) {
    // Reset sensor index for next loop
    sensorIndex = 0;

    // Switch mode to do the writes now
    modbusRequestMode = MODBUS_REQUEST_WRITE;
  } else {
    // Increment index to read the next sensor
    sensorIndex = sensorIndex + 1;
  }
}

// Send any write messages from the pumpStates array
// until they're all done, then switch the mode back to read
void requestNextWrite() {
  // Look up the Modbus address for this index
  int deviceAddress = pumpAddresses[pumpIndex];
  int state = pumpStates[pumpIndex];

  // Send a write register command to the pump
  requestWriteFor(deviceAddress, state);

  // Increment the device index, to lookup the next sensor address next time
  // Or go back to the start
  if (pumpIndex == numberOfPumps - 1) {
    // Reset pump index for next loop
    pumpIndex = 0;

    // Switch mode to do the reads again now
    modbusRequestMode = MODBUS_REQUEST_READ;
  } else {
    // Increment index to write to the next pump
    pumpIndex = pumpIndex + 1;
  }
}

// Request a read from one of the Modbus sensors
void requestReadFrom(int deviceAddress) {
  #ifdef DEBUG_REQUESTS
    Serial.print("READ from: addr-");
    Serial.println(deviceAddress);
  #endif

  if (!master.readInputRegisters(deviceAddress, 1, 1)) {
    #ifdef DEBUG_ERRORS
      Serial.print("Trouble requesting data from device: addr-");
      Serial.println(deviceAddress);
    #endif
  }
}

// Set power state for a pump by address
void requestWriteFor(int deviceAddress, int value) {
  #ifdef DEBUG_REQUESTS
    Serial.print("WRITE to: addr-");
    Serial.println(deviceAddress);
  #endif

  if (!master.writeSingleRegister(deviceAddress, 0, value)) {
    #ifdef DEBUG_ERRORS
      Serial.print("Trouble sending power command to device: addr-");
      Serial.println(deviceAddress);
    #endif
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
        #ifdef DEBUG_ERRORS
          // Debug message about the error
          Serial.print("Error response from device: addr-");
          Serial.print(deviceAddress);
          Serial.print(" Error code: ");
          Serial.println(response.getErrorCode());
        #endif

      } else {
        // Get the value from the response
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
  #ifdef DEBUG_RESPONSES
    Serial.print("Response from: addr-");
    Serial.print(deviceAddress);
    Serial.print(" Value: ");
    Serial.println(reading);
  #endif

  // We need to put the value in the sensorReadings array
  // To find the index in the array for this device address, loop through the addresses by index
  // and set the reading with the same index when the address matches
  for (int i = 0; i < numberOfSensors; i++) {
    if (sensorAddresses[i] == deviceAddress) {
      sensorReadings[i] = reading;
      break;
    }
  }

  // Not bothered about response from pumps unless printing them above in debug mode
}
