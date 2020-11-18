
/*
  Exploring how to handle responses when they arrive,
  and to request data from multiple devices
*/

#include <ModbusRTUMaster.h>
#include <RS485.h>

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

#define ADDR_PUMP_WEST 2
#define ADDR_PUMP_EAST 2 //CHANGE THIS TO 1 LATER
#define INDEX_PUMP_WEST 0
#define INDEX_PUMP_EAST 1

// Device states
#define DRIVE_PUMP_ON 1
#define DRIVE_PUMP_OFF 0

const int numberOfSensors = 3; // Update this if you add another one below
const int sensorAddresses[] = {ADDR_TEMP_WEST, ADDR_TEMP_EAST, ADDR_TEMP_WEST};
const int numberOfPumps = 2;
const int pumpAddresses[] = {ADDR_PUMP_WEST, ADDR_PUMP_EAST};

// Current device being read (index in the above array)
int sensorIndex = 0;
int pumpIndex = 0;

ModbusRTUMaster master(RS485);

// Timestamps to throttle infrequent operations
uint32_t lastSentTime = 0UL;
uint32_t lastPrintedTime = 0UL;

// Config
const uint32_t BAUDRATE = 9600UL;

// Save readings globally here
int westPanelTemp;
int eastPanelTemp;
int westPanelPipeTemp;
int eastPanelPipeTemp;
int poolTemp;
int airTemp;
int westPumpPressure;
int eastPumpPressure;
int systemPressure;

// pump state
int pumpStates[] = {DRIVE_PUMP_OFF, DRIVE_PUMP_OFF};

// define limiting parameters here

const int maxPoolTemp = 300; // 30degC
const int maxPanelTemp = 600; //(60degC)
const int minSystemPressure = 35; // (35mb)

int isReadingSensors = 1;

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
  if (millis() - lastSentTime > 50) {

    // Infrequently request data
    // Each time this is called, it will move to the next address in the addresses array
    if (isReadingSensors) {
      if (requestNextData()) {
        isReadingSensors = 0;
      }
    } else {
      if (requestNextCommand()) {
        isReadingSensors = 1;
      }
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

    Serial.print("Saved values: [West]: ");
    Serial.print(westPanelTemp);
    Serial.print(" [East]: ");
    Serial.println(eastPanelTemp);

    // Using the saved values, decide what to do
    decisionTree();

    lastPrintedTime = millis();
  }
}

// Replace this bit with the decision tree
void decisionTree() {
  // once we've got data check:
  // the line pressure is ok,
  // then whether the pool is too hot
  // then whether the west panel is too hot
  // then whether the west panel is hot enough to heat the pool
  // then again whether the panel is too hot
  // then whether the east panel is too hot
  // then whether the east panel is hot enough to heat the pool

  //do the west side first and chip in to do the actions every so often


  if (systemPressure < minSystemPressure) {

    // the line pressure is too low: switch the pumps off
    switchWestOff();

  } else {

    // the line pressure is ok, carry on :-)
    if (poolTemp > maxPoolTemp) {

      //  Pool is hot enough, check whether west panel is getting too hot
      if (westPanelTemp > maxPanelTemp) {

        // Whoops, west panel is getting too hot: cool the roof
        coolWestRoof();

      } else {

        // No, the panel is cool enough, stop pumping water through it to waste
        switchWestOff();
      }

    } else {

      // No the pool isn't too hot, is the west panel hotter than the pool?
      if (westPanelTemp > poolTemp) {

        // Yes, its hotter than the pool and the pool needs the heat
        heatPoolFromWest();

      } else {

        // No, the west panel is too cold
        switchWestOff();
      }

    }
  }




  // that deals with the west side, now look at east. Repeat the checks every so often...



  if (systemPressure < minSystemPressure) {

    // the line pressure is too low: switch the pumps off
    switchEastOff();

  } else {

    // the line pressure is ok, carry on :-)


    if (poolTemp > maxPoolTemp) {

      //  Pool is hot enough, check whether east panel is getting too hot
      if (eastPanelTemp > maxPanelTemp) {

        // Whoops, east panel is getting too hot, cool the roof
        coolEastRoof();

      } else {

        // No, the panel is cool enough, stop pumping water through it to waste
        switchEastOff();
      }

    } else {

      // No the pool isn't too hot, is the east panel hotter than the pool?
      if (eastPanelTemp > poolTemp) {

        // Yes, it's hotter than the pool and the pool needs the heat
        heatPoolFromEast();

      } else {

        // No, the panel is too cold so switch it off
        switchEastOff();
      }

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

int requestNextData() {
  // Look up the Modbus address for this index
  int deviceAddress = sensorAddresses[sensorIndex];

  // Send request to the sensor
  requestDataFrom(deviceAddress);

  // Increment the device index, to lookup the next sensor address next time
  // Or go back to the start
  if (sensorIndex == numberOfSensors - 1) {
    sensorIndex = 0;
    return 1;
  } else {
    sensorIndex = sensorIndex + 1;
    return 0;
  }
}

int requestNextCommand() {
  // Look up the Modbus address for this index
  int deviceAddress = pumpAddresses[pumpIndex];
  int state = pumpStates[pumpIndex];

  // Send command to the pump
  //   Serial.print("Sending command to addr-");
  //   Serial.println(deviceAddress);
  drivePumpFor(deviceAddress, state);

  // Increment the device index, to lookup the next sensor address next time
  // Or go back to the start
  if (pumpIndex == numberOfPumps - 1) {
    pumpIndex = 0;
    return 1;
  } else {
    pumpIndex = pumpIndex + 1;
    return 0;
  }

}
// Request data infrequently
// passing the address of the device to send the request to
void requestDataFrom(int deviceAddress) {
  //  Serial.print("Requesting data from: addr-");
  //  Serial.println(deviceAddress);

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
    case ADDR_TEMP_WEST: westPanelTemp = reading;
      break;
    case ADDR_TEMP_EAST: eastPanelTemp = reading;
      break;
    default:
      break;
  }
}
