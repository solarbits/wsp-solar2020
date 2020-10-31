/* THIS IS AN INITIAL STAB AT WRITING THE MAIN PROGRAM FOR THE SOLAR HEATING 
 * SYSTEM AT WOTTON POOL
 * It gets the panel and pool data, then decides whether to heat the pool.
 * Alan George 28 October 2020
 *  
 *  Function codes are declared and defined at the end
 */


// include the libraries we'll need throughout

#include <ModbusRTUMaster.h>
#include <RS485.h>
ModbusRTUMaster master(RS485);
const uint32_t baudrate = 9600UL;
int lastSentTime = 0;
int lastSentTime1 = 0;


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

 // declare the functions (they are defined lower down)

 //int GET_DATA(linePress, poolTemp, westPanelTemp, eastPanelTemp, airTemp);
 void SWITCH_WEST_OFF();
 void SWITCH_EAST_OFF();
 void COOL_WEST_ROOF();
 void COOL_EAST_ROOF();
 void HEAT_POOL_FROM_WEST();
 void HEAT_POOL_FROM_EAST();


//////////////////////////////////////////////////////////
void setup() {

  // start the serial port 
  Serial.begin(9600UL);
  // start the modbus master object :9600 baud, Halfduplex, 8 bits, no parity, 1 stop bit
  RS485.begin (9600, HALFDUPLEX, SERIAL_8N1);
  master.begin(9600);

  //decision tree diagnostics start
 /* 
 delay(8000); 
   while (Serial.available() > 0) {
linePress = Serial.parseInt();
Serial.print ("line pressure: ");
Serial.println (linePress);
delay(5000);
poolTemp = Serial.parseInt();
Serial.print ("pool temp: ");
Serial.println (poolTemp);
delay(5000);
westPanelTemp = Serial.parseInt();
Serial.print ("West Panel Temp: ");
Serial.println (westPanelTemp);
delay(5000);
eastPanelTemp = Serial.parseInt();
Serial.print ("East Panel Temp: ");
Serial.println (eastPanelTemp);

delay(5000);
   }
// decision tree diagnostics end
  */ 
}
/////////////////////////////////////////////////////////////
void loop() {
  // first get the data
 /* 
 //while (Serial.available() > 0) {
linePress = Serial.parseInt();
Serial.print ("line pressure: ");
Serial.println (linePress);
poolTemp = Serial.parseInt();
Serial.print ("pool temp: ");
Serial.println (poolTemp);
westPanelTemp = Serial.parseInt();
Serial.print ("West Panel Temp: ");
Serial.println (westPanelTemp);
eastPanelTemp = Serial.parseInt();
Serial.print ("East Panel Temp: ");
Serial.println (eastPanelTemp);

delay(5000);
*/
//  }

  

//int a = GET_DATA(linePress, poolTemp, westPanelTemp, eastPanelTemp, airTemp);
delay(5000);
GET_DATA();
        
   // then check: 
      // the line pressure, 
      // then whether the pool is too hot
      // then whether the west panel is too hot 
      // then whether the west panel is hot enough to heat the pool
      // then again whether the panel is too hot
      // then whether the east panel is too hot
      // then whether the east panel is hot enough to heat the pool

      if(linePress < minLinePress){

        // the line pressure is too low: switch the pumps off
   
       SWITCH_WEST_OFF();
       SWITCH_EAST_OFF();

      } else {

        // the line pressure is ok, carry on :-)

        if (poolTemp > maxPoolTemp) {

          //  Pool is hot enough, check whether west panel is getting too hot
           if (westPanelTemp > maxPanelTemp){

             // Whoops, west panel is getting too hot: cool the roof
             COOL_WEST_ROOF();
             
           } else {

             // No, the panel is cool enough, stop pumping water through it to waste
             SWITCH_WEST_OFF();  
         } 
         
        } else {

          // No the pool isn't too hot, is the west panel hotter than the pool?
          if (westPanelTemp > poolTemp){

            // Yes, its hotter than the pool and the pool needs the heat
            HEAT_POOL_FROM_WEST();
            
          } else {

            // No, the west panel is too cold
            SWITCH_WEST_OFF();
          }
            
        }

// that deals with the west side, now look at east. Repeat check on pool temperature...

      if (poolTemp > maxPoolTemp) {
      
        //  Pool is hot enough, check whether east panel is getting too hot
           if (eastPanelTemp > maxPanelTemp){

             // Whoops, east panel is getting too hot, cool the roof
             COOL_EAST_ROOF();
             
           } else {

             // No, the panel is cool enough, stop pumping water through it to waste
             SWITCH_EAST_OFF();         
           }
           
        } else {
          
          // No the pool isn't too hot, is the east panel hotter than the pool?
          if (eastPanelTemp > poolTemp){

            // Yes, it's hotter than the pool and the pool needs the heat
            HEAT_POOL_FROM_EAST();
            
          }else{

            // No, the panel is too cold so switch it off
            SWITCH_EAST_OFF();
          }  
       
      }
      
   }
}
////////////////////////////////////////////////////////////////////////////////
// Declare and define the functions 

//////////////////////////////////////////////////////
 void SWITCH_WEST_OFF() { Serial.println("W OFF");
 delay(1000);

  //switch the west pump (address 2) off
     if (!master.writeSingleRegister(2, 0, 0)) {
      // Failure treatment
  //    Serial.println ("modbus problem switching off west pump");
    }

      if (master.isWaitingResponse())  ModbusResponse response = master.available();

  //Hang on a second
  delay(1000);

  //close west flow valve
  digitalWrite(R0_8, LOW);

  //close west flow drain valve
  digitalWrite(R0_7, HIGH);

  //close west return valve
  digitalWrite(R0_5, LOW);

  //close west return drain valve
  digitalWrite(R0_6, HIGH);

}
//////////////////////////////////////////////////
 

 void SWITCH_EAST_OFF(){
  
 Serial.println("E OFF");
 delay(1000);
 
 //switch the east pump (address 1) off
     if (!master.writeSingleRegister(1, 0, 0)) {
      // Failure treatment
  //    Serial.println ("modbus problem switching off east pump");
    }

  if (master.isWaitingResponse()) ModbusResponse response = master.available();

  //Hang on a second
  delay(1000);

  //close east flow valve
  digitalWrite(R0_4, LOW);

  //close east flow drain valve
  digitalWrite(R0_3, HIGH);

  //close east return valve
  digitalWrite(R0_1, LOW);

  //close east return drain valve
  digitalWrite(R0_2, HIGH); 
 }

//////////////////////////////////////////////////////

 
 void COOL_WEST_ROOF() {
  Serial.println("Cool W");
 delay(1000);

 //switch the west pump (address 2) on
     if (!master.writeSingleRegister(2, 0, 1)) {
      // Failure treatment
  //    Serial.println ("modbus problem switching on west pump");
    }

  if (master.isWaitingResponse()) ModbusResponse response = master.available();

  //Hang on a second
  delay(1000);

  //open west flow valve
  digitalWrite(R0_8, HIGH);

  //close west flow drain valve
  digitalWrite(R0_7, HIGH);

  //close west return valve
  digitalWrite(R0_5, LOW);

  //open west return drain valve
  digitalWrite(R0_6, LOW);
 }

///////////////////////////////////////////////////////////

 
 void COOL_EAST_ROOF() {
  
 Serial.println("Cool E");
 delay(1000);
 //switch the east pump (address 1) on
     if (!master.writeSingleRegister(1, 0, 1)) {
      // Failure treatment
  //    Serial.println ("modbus problem switching on east pump");
    }

  if (master.isWaitingResponse()) ModbusResponse response = master.available();

  //Hang on a second
  delay(1000);

  //open east flow valve
  digitalWrite(R0_4, HIGH);

  //close east flow drain valve
  digitalWrite(R0_3, HIGH);

  //close east return valve
  digitalWrite(R0_1, LOW);

  //open east return drain valve
  digitalWrite(R0_2, LOW);
 
 }

//////////////////////////////////////////////////////////
 
 void HEAT_POOL_FROM_WEST(){
  Serial.println("Heat f W");
 delay(1000);

 //switch the west pump (address 2) on
     if (!master.writeSingleRegister(2, 0, 1)) {
      // Failure treatment
  //    Serial.println ("modbus problem switching on west pump");
    }

  if (master.isWaitingResponse()) ModbusResponse response = master.available();

  //Hang on a second
  delay(1000);

  //open west flow valve
  digitalWrite(R0_8, HIGH);

  //close west flow drain valve
  digitalWrite(R0_7, HIGH);

  //open west return valve
  digitalWrite(R0_5, HIGH);

  //close west return drain valve
  digitalWrite(R0_6, HIGH);
 
 
 }

//////////////////////////////////////////////////////

 
 void HEAT_POOL_FROM_EAST() {
  
  Serial.println("Heat f E");
 delay(1000);
 //switch the east pump (address 1) on
     if (!master.writeSingleRegister(1, 0, 1)) {
      // Failure treatment
  //    Serial.println ("modbus problem switching on east pump");
    }
    
  if (master.isWaitingResponse()) ModbusResponse response = master.available();
  
  //Hang on a second
  delay(1000);

  //open east flow valve
  digitalWrite(R0_4, HIGH);

  //close east flow drain valve
  digitalWrite(R0_3, HIGH);

  //open east return valve
  digitalWrite(R0_1, HIGH);

  //close east return drain valve
  digitalWrite(R0_2, HIGH);
 
 }
 ///////////////////////////////////////////////////

 // Get-data tests (from papouches) function 

 void GET_DATA(){
  
 
    // Send a request every 1000ms
  if (millis() - lastSentTime > 1000) {
    if (!master.readInputRegisters(33, 1,1)) {  Serial.print ("slow ");
    }
    lastSentTime = millis();
  } 
  if (master.isWaitingResponse()) {
    ModbusResponse response = master.available();
    if (response) {
      if (response.hasError()) {
        // Response failure treatment. Use Serial.print(response.getErrorCode());to get code.
      } else {
          // Get the input registers values from the response
          Serial.print("West Panel Temperature: ");
  //        for (int i = 0; i < 2; ++i) 
  {
            westPanelTemp = (response.getRegister(0));
            Serial.print(westPanelTemp);
          }
          Serial.println();
      }
    }
  }
  delay (1000);
   // Send a request every 1000ms
  if (millis() - lastSentTime1 > 1000) {
    if (!master.readInputRegisters(23, 1,1)) {  Serial.print ("slow1 ");
    }
    lastSentTime1 = millis();
  } 
  if (master.isWaitingResponse()) {
    ModbusResponse response = master.available();
    if (response) {
      if (response.hasError()) {
        // Response failure treatment. Use Serial.print(response.getErrorCode());to get code.
      } else {
          // Get the input registers values from the response
          Serial.print("Pool Temperature: ");
  //        for (int i = 0; i < 2; ++i) 
  {
            poolTemp = (response.getRegister(0));
            Serial.print(poolTemp);
          }
          Serial.println();
      }
    }
  }
  delay(1000);
 }
