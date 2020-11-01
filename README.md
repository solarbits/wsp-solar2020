# wsp-solar2020
Generates new firmware for M-Duino 19+ to simplify coding for a less code-aware audience/maintenance

This code is a bit messy at the moment.

Tthe next tehe first 40 lines set up the libraries needed and initialise the main variables (pressures and temperatures). Lines 30 to 40 declare the 7 main functions.  The first is to collect the pressure and temperature data from the sensors using modbus.  The remaining 6 are the functions that take action -- 3 for each side (west and east): switching the pump (on or off using Modbus) and controlling the 4 mains-operated motorised valves using the Mduiuno's relays.

The next ten or so lines are the void setup() function and initialise th eoperation of the serial port and the Modbus function.

The repeating code starts at about line 100 and starts with GET_DATA so that the system has up-to-date pressures and temperatures to work with.  The code then drops through a range of decisions (IF-ELSEs) the first being to check whether there is line pressure in the suction side of the pumps.  If not the pumps could run dry and be damaged. So it switches everything off if the supply pressure tot the pumps is too low
Then there a set of decisions for the west panel followed by the same decisions for the east panel.  At each point one of the control functions is called.
Finally at about line 200 (through to 450) the seven functions are defined

I will upl;oad a system line diagram and the decision tree flow chart in separate files
