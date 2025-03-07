/*
  Uses modbus to collect sensor data (2 temps and 3 PTs) and to write to the 2 inverters
  plus code added to send data to node red via a remote broker
  plus code added to tell node red the state of the roofs
  plus code added to calculate power
  plus code to skip obviously spurious data
  includes pH and Cl measurements
  was uploaded on site on 7/3/21
  Revised in Dec 2024, to change MQTT broker and reduce pump hazard pressure to 20mb
*/
#include <Ethernet.h>
#include <ModbusRTUMaster.h>
#include <RS485.h>

// libraries needed for MQTT communication
#include <ArduinoJson.h>
#include <PubSubClient.h>
#define MQTT_ID "plc1"
#define MQTT_USER "mqtt"
#define MQTT_PASS "123mqtt"

byte mac[] = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xAE };

// Local
//IPAddress broker(192, 168, 0, 128);

// Digital ocean
// IPAddress broker(178, 62, 65, 122);

// Fly
IPAddress broker(137, 66, 31, 103);


unsigned port = 1883;
// Initialize client
EthernetClient client;
PubSubClient mqtt(client);

//#define DEBUG_REQUESTS 1 // Uncomment to enable
//#define DEBUG_RESPONSES 1 // Comment to disable
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
#define INDEX_PRES_WEST 6
#define INDEX_PRES_EAST 7
#define INDEX_PRES_SYSTEM 8

#define ADDR_PUMP_WEST 2
#define ADDR_PUMP_EAST 1
// Indexes to look these up in the state array
#define INDEX_PUMP_WEST 0
#define INDEX_PUMP_EAST 1

// Device states
#define DRIVE_PUMP_ON 1
#define DRIVE_PUMP_OFF 0

// Roof states (same for each side)
#define LOW_SYST_PRES   0
#define POOL_TOO_HOT    1
#define COOL_ROOF       2
#define HEAT_POOL       3
#define AWAIT_SUNSHINE  4
// Indexes to look these up in the state array
#define INDEX_ROOF_WEST 0
#define INDEX_ROOF_EAST 1

const int numberOfSensors = 9; // Update this if you add another one below
const int sensorAddresses[numberOfSensors] = {ADDR_TEMP_WEST, ADDR_TEMP_WEST_PIPE, ADDR_TEMP_EAST,
                                              ADDR_TEMP_EAST_PIPE, ADDR_TEMP_POOL, ADDR_TEMP_AIR,
                                              ADDR_PRES_WEST, ADDR_PRES_EAST, ADDR_PRES_SYST
                                             };
const int numberOfPumps = 2;
const int pumpAddresses[numberOfPumps] = {ADDR_PUMP_WEST, ADDR_PUMP_EAST};

const int numberOfRoofs = 2;

const int numberOfChems = 2;

// Current device being read (index in the above array)
int sensorIndex = 0;
int pumpIndex = 0;

// Written into here by decision tree, then looked up by index sequentially by the modbus send loop
int pumpStates[numberOfPumps] = {DRIVE_PUMP_OFF, DRIVE_PUMP_OFF};
// Written into by receiving responses from MB sensors
int sensorReadings[numberOfSensors] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
// Written into here by decision tree west is first element, east second
int roofStates[numberOfRoofs] = {0, 0};
// Power (calculated later in decaWatts -- to keep integer field under 32,000)
int roofPowers[numberOfRoofs] = {0, 0};
// Chemical concnetrations measured later and written into here
int chemConcentrations[numberOfChems] = {0, 0};

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
const int minSystemPressure = 20; // (20mb)

/////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);

  //  MQTT stuff first

  Ethernet.begin(mac);
  Serial.print("Connect to ethernet ");
  Serial.println(Ethernet.localIP());
  mqtt.setServer(broker, port);
  Serial.println("Connecting to MQTT");
  reconnect();
  if(mqtt.connected()) {
    Serial.println("Connected to MQTT");
  } else {
    Serial.println("Not connected to MQTT");
  }
  

  // Now the modbus bits

  RS485.begin(BAUDRATE, HALFDUPLEX, SERIAL_8N1);
  master.begin(BAUDRATE);
  Serial.println("Starting modbus");
}
//////////////////////////////////////////////////////////////////////////////
void loop() {

  // Check that its been a few tens of mS since last request for data
  // This is like delay(80) but doesn't block the loop
  // The loop should keep looping incase a response comes along
  if (millis() - lastSentTime > 80) {

    // Then infrequently request data
    // Each time these request functions are called, it will move to the next address in the addresses array until switching mode when the array is complete
    switch (modbusRequestMode) {
      case MODBUS_REQUEST_READ:
        requestNextRead();
        break;
      case MODBUS_REQUEST_WRITE:
        requestNextWrite();
        break;
    }
    //  And keep MQTT connected
    if (!mqtt.connected()) {
      Serial.println("MQTT disconnected");
      reconnect();
    }
    else {
      mqtt.loop();
    }
    lastSentTime = millis();
  }

  // Check to see if a response is waiting every loop
  // This function will print the result if anything comes
  listenForResponse();

  // Same again here. Periodically, and unblockingly print out whatever is saved globally
  // This will be what is the state of the saved vars on which we decide whether to run
  // the pumps or not. This happens every 5 seconds
  if (millis() - lastPrintedTime > 5000) {
    Serial.println();
    Serial.print("Saved values: ");
    for (int i = 0; i < numberOfSensors; i++) {
      Serial.print(sensorReadings[i]);
      Serial.print(", ");
    }
    Serial.println();

    // Using the saved values, decide what to do
    // and set new pump states based on temperature comparisons
    decisionTree();
    // Also get the analog chemistry data
    readChemistry();

    for (int i = 0; i < numberOfRoofs; i++) {
      if (roofStates[i] != HEAT_POOL)  roofPowers[i] = 0;
    }
    //      and send data to node red
    for (int i = 0; i < numberOfSensors; i++) {
      publishInput(0, i, sensorReadings[i]);
    }
    for (int i = 0; i < numberOfPumps; i++) {
      publishInput(1, i, pumpStates[i]);
    }
    for (int i = 0; i < numberOfRoofs; i++) {
      publishInput(2, i, roofStates[i]);
      publishInput(3, i, roofPowers[i]);
    }
    for (int i = 0; i < numberOfChems; i++) {
      publishInput(4, i, chemConcentrations[i]);
    }
    lastPrintedTime = millis();
  }
}

//////////////////////////////////////////////////////////////////////////
// Using the saved values, decide what to do
// and set new pump states and set relays
// based on temperature comparisons
void decisionTree() {
  // check the line pressure is ok,
  // Turn off the pumps and bail out if not
  // Otherwise check:
  // whether the pool is too hot
  // then whether the west panel is too hot
  // then whether the west panel is hot enough to heat the pool
  // then again whether the panel is too hot
  // then whether the east panel is too hot
  // then whether the east panel is hot enough to heat the pool
  if (sensorReadings[INDEX_PRES_SYSTEM] < minSystemPressure) {
    // the line pressure is too low: switch the pumps off
    switchWestOff();
    switchEastOff();
    roofStates[INDEX_ROOF_WEST] = LOW_SYST_PRES;
    roofStates[INDEX_ROOF_EAST] = LOW_SYST_PRES;

    return;
  }
  // the line pressure is ok, carry on :-)
  if (sensorReadings[INDEX_TEMP_POOL] > maxPoolTemp) {
    roofStates[INDEX_ROOF_WEST] = POOL_TOO_HOT;
    //  Pool is hot enough, check whether west panel is getting too hot
    if (sensorReadings[INDEX_TEMP_WEST] > maxPanelTemp) {
      // Whoops, west panel is getting too hot: cool the roof
      roofStates[INDEX_ROOF_WEST] = COOL_ROOF;
      coolWestRoof();
    } else {
      // No, the panel is cool enough, stop pumping water through it to waste
      switchWestOff();
    }
  } else {
    // No the pool isn't too hot, is the west panel 0.5 degrees hotter than the pool?
    if (sensorReadings[INDEX_TEMP_WEST] > (5 + sensorReadings[INDEX_TEMP_POOL])) {
      // Yes, its hotter than the pool and the pool needs the heat
      heatPoolFromWest();
      roofStates[INDEX_ROOF_WEST] = HEAT_POOL;
      // and calculate the power
      // Pumps deliver 20 litres/min which is 333 gm/s. Remember temp is in 0.1 DegCs
      // power in decaWatts is therefore Delta T x 4.2 x 333 / 10 (ie DT x 14)
      roofPowers[INDEX_ROOF_WEST] = (sensorReadings[INDEX_TEMP_WEST] - sensorReadings[INDEX_TEMP_POOL]) * 14;
    } else {
      // No, the west panel is too cold
      switchWestOff();
      roofStates[INDEX_ROOF_WEST] = AWAIT_SUNSHINE;
    }
  }
  // that deals with the west side, now look at east. Repeat the checks every so often...
  if (sensorReadings[INDEX_TEMP_POOL] > maxPoolTemp) {
    roofStates[INDEX_ROOF_EAST] = POOL_TOO_HOT;
    //  Pool is hot enough, check whether east panel is getting too hot
    if (sensorReadings[INDEX_TEMP_EAST] > maxPanelTemp) {
      // Whoops, east panel is getting too hot, cool the roof
      roofStates[INDEX_ROOF_EAST] = COOL_ROOF;
      coolEastRoof();
    } else {
      // No, the panel is cool enough, stop pumping water through it to waste
      switchEastOff();
    }
  } else {
    // No the pool isn't too hot, is the east panel half a degree hotter than the pool?
    if (sensorReadings[INDEX_TEMP_EAST] > (5 + sensorReadings[INDEX_TEMP_POOL])) {
      // Yes, it's hotter than the pool and the pool needs the heat
      roofStates[INDEX_ROOF_EAST] = HEAT_POOL;
      heatPoolFromEast();
      // and calculate the power
      // Pumps deliver 20 litres/min which is 333 gm/s. Remember temp is in 0.1 DegCs
      // power in decaWatts is therefore Delta T x 4.2 x 333 / 10 (ie DT x 14)
      roofPowers[INDEX_ROOF_EAST] = (sensorReadings[INDEX_TEMP_EAST] - sensorReadings[INDEX_TEMP_POOL]) * 14;
    } else {
      // No, the panel is too cold so switch it off
      roofStates[INDEX_ROOF_EAST] = AWAIT_SUNSHINE;
      switchEastOff();
    }
  }
}
//////////////////////////////////////////////////////////////////////////
void switchWestOff() {
  Serial.println("Switch west off");
  pumpStates[INDEX_PUMP_WEST] = DRIVE_PUMP_OFF;

  //close west flow valve
  digitalWrite(R0_8, LOW);

  //close west flow drain valve
  digitalWrite(R0_7, HIGH);

  //close west return valve
  digitalWrite(R0_5, LOW);

  //close west return drain valve
  digitalWrite(R0_6, HIGH);
}
/////////////////////////////////////////////////////////////////////////
void switchEastOff() {
  Serial.println("Switch east off");
  pumpStates[INDEX_PUMP_EAST] = DRIVE_PUMP_OFF;

  //close east flow valve
  digitalWrite(R0_4, LOW);

  //close east flow drain valve
  digitalWrite(R0_3, HIGH);

  //close east return valve
  digitalWrite(R0_1, LOW);

  //close east return drain valve
  digitalWrite(R0_2, HIGH);
}
/////////////////////////////////////////////////////////////////////////
void coolWestRoof() {
  Serial.println("Cool panel west");
  pumpStates[INDEX_PUMP_WEST] = DRIVE_PUMP_ON;
  //open west flow valve
  digitalWrite(R0_8, HIGH);

  //close west flow drain valve
  digitalWrite(R0_7, HIGH);

  //close west return valve
  digitalWrite(R0_5, LOW);

  //open west return drain valve
  digitalWrite(R0_6, LOW);
}
/////////////////////////////////////////////////////////////////////////
void coolEastRoof() {
  Serial.println("Cool panel east");
  pumpStates[INDEX_PUMP_EAST] = DRIVE_PUMP_ON;
  //open east flow valve
  digitalWrite(R0_4, HIGH);

  //close east flow drain valve
  digitalWrite(R0_3, HIGH);

  //close east return valve
  digitalWrite(R0_1, LOW);

  //open east return drain valve
  digitalWrite(R0_2, LOW);

}
//////////////////////////////////////////////////////////////////////////
void heatPoolFromWest() {
  Serial.println("Heat pool from west");
  pumpStates[INDEX_PUMP_WEST] = DRIVE_PUMP_ON;
  //open west flow valve
  digitalWrite(R0_8, HIGH);

  //close west flow drain valve
  digitalWrite(R0_7, HIGH);

  //open west return valve
  digitalWrite(R0_5, HIGH);

  //close west return drain valve
  digitalWrite(R0_6, HIGH);
}
///////////////////////////////////////////////////////////////////////////
void heatPoolFromEast() {
  Serial.println("Heat pool from east");
  pumpStates[INDEX_PUMP_EAST] = DRIVE_PUMP_ON;
  //open east flow valve
  digitalWrite(R0_4, HIGH);

  //close east flow drain valve
  digitalWrite(R0_3, HIGH);

  //open east return valve
  digitalWrite(R0_1, HIGH);

  //close east return drain valve
  digitalWrite(R0_2, HIGH);
}
//////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////////////////
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
    Serial.print(".");
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
  //  first check whether it is a pressure and if not it must be a temperature
  if (sensorIndex >= 6) {
    if (!master.readInputRegisters(deviceAddress, 8, 2)) {
#ifdef DEBUG_ERRORS
      Serial.print("Trouble requesting data from device: addr-");
      Serial.println(deviceAddress);
#endif
    }
  } else {
    // it must be a temperature
    if (!master.readInputRegisters(deviceAddress, 1, 1)) {
#ifdef DEBUG_ERRORS
      Serial.print("Trouble requesting data from device: addr-");
      Serial.println(deviceAddress);
#endif
    }
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
//////////////////////////////////////////////////////////////////////////////////
// Pressure transcucers return a 32bit float over 2 registers
// This function takes the registers and shifts them together to reassemble the float value
// For use in the system we can * 1000 to turn that into an integer value of millibars
// to be consistent with other readings
int convert32BitFloatToInt(ModbusResponse response) {
  uint16_t reg8 = (response.getRegister(0));
  uint16_t reg9 = (response.getRegister(1));

  byte msb2 = reg8;
  byte msb3 = reg8 >> 8;
  byte msb0 = reg9;
  byte msb1 = reg9 >> 8;

  float x;
  ((byte*)&x)[3] = msb3;
  ((byte*)&x)[2] = msb2;
  ((byte*)&x)[1] = msb1;
  ((byte*)&x)[0] = msb0;

  int p = x * 1000.0;
  return p;
}
///////////////////////////////////////////////////////////////////////////////////////
// Check often if a response is waiting
// If there isn't this will just do nothing and the loop continues
// until something happens
void listenForResponse () {
  int reading;
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
        // No error, handle the data

        // Some devices return 32bit data across 2 registers, which needs to be converted
        // For most sensors we can just store whats in the first register
        switch (deviceAddress) {
          case ADDR_PRES_EAST:
          case ADDR_PRES_WEST:
          case ADDR_PRES_SYST:
            reading = convert32BitFloatToInt(response);
            break;
          default:
            reading = response.getRegister(0);
            break;
        }

        // So now we have the reading, and the address that it came from
        // so it can be dealt with, eg. Save it into the right variable
        handleResponseData(deviceAddress, reading);
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////////////////
// Response packet comes back with the address and the reading.
// This function will print it for debugging, and saves it to the tempEast and tempWest vars
void handleResponseData (int deviceAddress, int reading) {
#ifdef DEBUG_RESPONSES
  Serial.print("Response from: addr-");
  Serial.print(deviceAddress);
  Serial.print(" Value: ");
  Serial.println(reading);
#endif

  // Identify and skip any dopey readings (less than -50C or -500mb OR greater than 300C or 3000mb)
  if ((reading < -500) or (reading > 3000)) return;
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

// Add some functions to run node red through MQTT
///////////////////////////////////////////////////////////////////////////////////////
void reconnect() {
  if (mqtt.connect(MQTT_ID, MQTT_USER, MQTT_PASS)) {

  } else {
    // MQTT connect fail
    client.stop();
  }
}

//////////////////////////////////////////////////////////////////////////////////////
void publishInput(int zone, int index, int value) {
  DynamicJsonBuffer json(JSON_OBJECT_SIZE(3));
  JsonObject &root = json.createObject();
  if (root.success()) {
    root["zone"] = zone;
    root["index"] = index;
    root["value"] = value;
    publish("IP65", root);
  }
}
/////////////////////////////////////////////////////////////////////////////////////
void publish(const char *topic, JsonObject & root) {
  unsigned len = root.measureLength();
  if (len > 0) {
    char *payload = new char[len + 1];
    if (payload) {
      root.printTo(payload, len + 1);
      publish(topic, payload);
      delete[] payload;
    }
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void publish( char *topic, char *payload) {
  if (mqtt.connected()) {
    mqtt.publish(topic, payload);
    Serial.print("publishing ");
    Serial.print(topic);
    Serial.print(" ");
    Serial.println(payload);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////
void readChemistry() {
  // The pH scale on the Jesco doser goes from 2 to 12 and corresponds to 0 to 20 mA (not 4-20mA)
  // put the current through 470 ohms (the doser has a maximum tolerable resistance of 500 ohms)
  // so the output of the doser will range from 0.00 V to 9.40 V
  // M-duino ADC has a 0-10V range for numbers 0-1023, so the available range for pH 2 to 12 is 0 to (0.94 x 1023), 0-962
  // 1/962 = 0.00104
  // calculate the pH in hundredths
  int pH100 = 200 + (analogRead(I0_2) * 1.04);
  chemConcentrations[0] = pH100;
  // The Chlorine scale on the Jesco doser goes from 0 ppm to 4 ppm and corresponds to 0 to 20 mA (not 4-20mA)
  // put the current through 470 ohms (the doser has a maximum tolerable resistance of 500 ohms)
  // so the output of the doser will range from 0.00 V to 9.40 V
  // M-duino ADC has a 0-10V range for numbers 0-1023, so the available range for pH 2 to 12 is 0 to (0.94 x 1023), 0-962
  // 1/962 = 0.00104
  // calculate the Chlorine concentration in parts per 10,000
  int chlor100 = (analogRead(I0_3) * 4 * 0.104);
  chemConcentrations[1] = chlor100;
    Serial.print("chemistry values: ");
  for (int i = 0; i < numberOfChems; i++) {
    Serial.print(chemConcentrations[i]);
    Serial.print(", ");
  }
  Serial.println();
}
