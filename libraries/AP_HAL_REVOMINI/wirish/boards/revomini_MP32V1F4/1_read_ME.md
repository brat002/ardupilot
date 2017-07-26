I tried to maintain compatibility with the OpenPilot documentation. The main difference - FlexiPort can 
be Serial and external I2C port is on pins 7&8 of Input port. But this can be changed by HAL_FLEXI_I2C parameter


Main Port - telemetry, Serial1. As a variant it can be used as SBUS input with hardware inverter (Parameter HAL_UART1_SBUS)
FlexiPort - OSD, Serial2


Input Port - PWM input is not supported - this is general trend
pin 1 of Input port is GND
pin 2 of Input port is +5
pin 3 of Input port is 1st PPM/SBUS/DSM input or Servo9 (if you use RC via UART)
pin 4 of Input port is 2nd PPM/SBUS/DSM input or Servo10 - same
pins 5&6 of Input port are Tx and Rx of Serial3 (for GPS)
pins 7&8 of Input port are SCL and SDA of external I2C (or Tx and Rx for SoftSerial) - or Servos 7&8


Output Port for MOTORs
Connect to PWM output pins in ArduCopter, CleanFlight or OpenPilot order, and set parameter HAL_MOTOR_LAYOUT accordingly

5&6 PWM Output pins are Rx and Tx of Serial4 - but only for quads or planes

also pins 1&2 of OutputPort can be used as servos, in this case connect motors to pins 3-6 in ArduCopter order


PWM input is not supported - this is general trend



OpLink port

DSM satellite can be connected to Oplink port (hardware Serial5) or to PPM inputs (pins 3&4 of input port)

binding of DSM satellite can be done in 2 ways:
1. with some additional hardware - managed stabilizer 3.3 volts. 
2. directly connected to 3.3v, binding will require short power off

Connection to OpLink port
Pin 1 is Gnd, 
pin 2 is +5(DSM sat requires 3.3!)
pin 3 is Rx 
pin 4 is Enable for 3.3 stab.

