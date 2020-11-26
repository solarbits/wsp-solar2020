/*
   Copyright (c) 2018 Boot&Work Corp., S.L. All rights reserved
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <ModbusRTUMaster.h>

// Define the ModbusRTUMaster object, using the RS-485 or RS-232 port (depending on availability)
#if defined HAVE_RS485_HARD
#include <RS485.h>
ModbusRTUMaster master(RS485);
#endif

int payload[] = {0,0};
uint32_t lastSentTime = 0UL;
const uint32_t baudrate = 9600UL;

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600UL);

  // Start the serial port
#if defined HAVE_RS485_HARD
  RS485.begin(baudrate, HALFDUPLEX, SERIAL_8N1);

#endif
 int val;
  // Start the modbus master object.
  // It is possible to define the port rate (default: 19200)
  master.begin(baudrate);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  // Send a request every 1000ms
  if (millis() - lastSentTime > 1000) {
    // Send a Read Coils request to the slave with address 31
    // It requests for 23 coils starting at address 10
    // IMPORTANT: all read and write functions start a Modbus transmission, but they are not
    // blocking, so you can continue the program while the Modbus functions work. To check for
    // available responses, call master.available() function often
       
    if (!master.readInputRegisters(64, 8, 2)) {
      // Failure treatment
      Serial.println("oops");
    }
    lastSentTime = millis();
  }

  // Check available responses often
  if (master.isWaitingResponse()) {
     ModbusResponse response = master.available();
    if (response) {
      if (response.hasError()) {
        // Response failure treatment. You can use 
        Serial.print(response.getErrorCode());
        // to get the error code.
      } else {
          
          
  uint16_t reg8 = (response.getRegister(0));
  uint16_t reg9 = (response.getRegister(1));

    byte msb2 = reg8;
    byte msb3 = reg8 >> 8;
    byte msb0 = reg9;
    byte msb1 = reg9 >> 8;

    float x;
    ((byte*)&x)[3]= msb3;
    ((byte*)&x)[2]= msb2;
    ((byte*)&x)[1]= msb1;
    ((byte*)&x)[0]= msb0;

    x = x * 1000.0;
            
   //         Serial.print(',');
          Serial.print(x);
          Serial.println();
          }  
      }
   }
}
