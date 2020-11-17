/*
  this is to prototype the GET_DATA function
*/

#include <ModbusRTUMaster.h>

// Define the ModbusRTUMaster object, using the RS-485 or RS-232 port (depending on availability)

#include <RS485.h>
ModbusRTUMaster master(RS485);


uint32_t lastSentTime = 0UL;
uint32_t lastSentTime1 = 0UL;
bool requestGateOpen = true; bool checkResponseGateOpen = false;
bool modbus23Needed = true;
bool modbus33Needed = false;
bool modbus64Needed = false;
bool modbus21Needed = false;

bool dunnit = false;
const uint32_t baudrate = 9600UL;

// set up key parameters
// pressure in mb, temp in decidegrees (0.1 degsC)
int linePress = 76;
int poolTemp = 0;
int westPanelTemp = 0;
int eastPanelTemp = 0;
int airTemp = 0;
int maxPoolTemp = 300;
int maxPanelTemp = 600;
int minLinePress = 35;
int i = 0;
int dev = 0;

int addresses[] = {33, 23, 33};
int deviceIndex = 0;

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600UL);
  RS485.begin(baudrate, HALFDUPLEX, SERIAL_8N1);
  master.begin(baudrate);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {

  getData(deviceIndex);


  if (checkResponseGateOpen) {


    // Check available responses often
    if (master.isWaitingResponse()) {
      ModbusResponse response = master.available();
      if (response) {
        if (response.hasError()) {
          // Response failure treatment. You can use
          Serial.println(response.getErrorCode());
          // to get the error code.
        } else {
          // Get the coil value from the response
          int coil = response.getRegister(0);
          Serial.print("Coil : ");
          Serial.print(addresses[deviceIndex]);
          Serial.print(deviceIndex);
          Serial.println(coil);
          // close this check loop
          checkResponseGateOpen = false;
          // and open the path for next user...
          requestGateOpen = true;
          if (deviceIndex == 1) {
            deviceIndex = 0;

          } else {
            deviceIndex = deviceIndex + 1;


          }
        }
      }
    }
  }
}



//////////////////////////////////////////////////////////////////////////////////////

// . HERE IS THE EXAMPLE FUNCTION GET_DATA

/////////////////////////////////////////////////////////////////////////////
int getData(int deviceIndex) {
  int deviceAddress = addresses[deviceIndex];

  // do the modbus request, prevent further requests and allow checks to be made
  if (requestGateOpen) {
    requestGateOpen = false; checkResponseGateOpen = true;
    if (!master.readInputRegisters(deviceAddress, 1, 1)) {
      //Error treatment
      Serial.println("trouble with reading");
    }
    delay (20);
  }
}
//////////////////////////////////////////////////////////////////////////////////////
