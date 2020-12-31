# wsp-solar2020
This GITHUB entry describes the code and engineering details for a solar heating system at Wotton Pool.  The heating is to warm the pool water and uses two panels: one on the west face of a pitched roof and the other on the east face.  By good fortune (not design!) the ridge of the roof runs exactly north-south.  There are two identical mechanical systems so the single control system is uses 'west' or 'east' to identify the relavant valves, pumps etc.

The code was written to generate new firmware for M-Duino 19+ programmable logic controller to simplify coding for a less code-aware audience/maintenance.  The code used is C++ and uses a number of simplifying utility functions provided by the plc supplier (Boot and Work, Barcelona)

The physical situation is described in 'PANELS3.pdf' and shows the flow circuit and the panels and pump layout.  The C&I electrical system layout (which shows the positions and identity of the pumps and sensors) is shown in 'Electrical diagram 8_8_2019.pdf'.  

The sensors are all digital and communicate using MODBUS RTU.  Similarly the pumps are activated through inverters using MODBUS RTU.  There are mains-operated motorised valves that are driven directly through 8 integral relays in the PLC

The decisions whether/how to operate the panel, which involves instructing the pumps and the valves is done in a C++ function 'decisionTree()'.  The logic is displayed in 'flow chart20_10_20 (1).pdf' It shows the 5 states each roof can be put into. One of the states is to switch everything off if the pressure in the suction side of the solar pumps is low.  This is to inhibit the operation of the pumps when there is (for any reason) no water available on their inputs and the pumps would otherwise run dry and damage themselves.  The other states on the flow chart are self-explanatory.

The plc also has code to broadcast the sensor data, the on/off state of the pumps and the operational status of the roof.  It does this through an ethernet connection to an MQTT broker which is listened to by Node Red.  This allows a data dashboard to be available for remote monitoring of the system.

The way that the code operates is shown in 'Solar software architecture.pdf'
