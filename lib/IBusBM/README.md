# Arduino Flysky/Turnigy RC iBus protocol handler
Arduino library for Flysky/Turnigy RC iBUS protocol - servo (receive) and sensors/telemetry (send) using hardware UART.

The iBUS protocol is a half-duplex protocol developed by Flysky to control multiple servos and motors using a single digital line. The values received for each servo channel are between 1000 (hex eE8) and 2000 (hex 7D0) with neutral sub trim setting, which corresponds with the pulse width in microseconds for most servos.

The protocol can also connect sensors to send back telemetry information to a RC transceiver. Depending on your transmitter you can use multiple sensors. You can define up to 10 sensors using this library. The Turnigy FS-MT6 only supports voltage, temperature, motor speed and pressure, but OpenTX based receivers support a long list of sensors, which can all be used by passing the right sensor ID to the addSensor() function.

This library was written and tested for the TGY-IA6B receiver and should work for other receivers too (such as the FS-iA10 and TGY-iA10).
The TGY-IA6B has 2 iBUS pins: one for the servos (only output) and one for the sensors/telemetry
(which uses a half-duplex protocol to switch between output and input to poll for sensor data).

Receivers with one iBUS pin typically send servo commands over the iBUS line, but do not always poll external sensors. The TGY-iA6C for instance only sends servo data over the iBUS and is only able to send back internal telemetry data (such as Rssi and voltage) and the voltage measurement of the B-DET connection. The specs of the Flysky X6B and Flysky FS-iA8X are unclear if telemetry sensors is supported over iBUS and they are not (yet) tested with this library.

## Getting Started

To install this library use the **Clone or download > Download ZIP** button on the repository home page and then install the library in your Arduino environment using **Sketch > Include Library > Add .ZIP Library...**

This library supports AVR based Arduino boards (Arduino MEGA, UNO, Nano, Micro, etc.), ESP32 based boards (ESP32, NodeMCU, etc.), STM32 boards (STM32F103, etc.) and MBED (such as the Arduino NANO 33 BLE).

### Prerequisites

The iBUS library requires a dedicated hardware serial (UART) port on the Arduino board. If your board only has one UART port you can still use that port for serial debug communication with your PC as long as you plan to use servo output mode only (the baud rate will be fixed at 115200 baud and you should only attach the UART TX pin to the USB-Serial converter). The ATMEGA boards typically have more than one UART port.

You have three options:
- If you plan to **only use servo output** iBUS data in your sketch, you only need to connect the iBUS servo output pin from your receiver to the RX pin of the allocated UART. The TX pin of the UART will not be used by the library as there is no information sent back to the RC receiver.
- If you plan to **only provide sensor** iBUS data from your sketch, you will need to connect the iBUS sensor pin to both the RX and TX pin of the allocated UART. You need to include a diode (such as 1N4148) between the Arduino TX pin and the wire between the iBUS pin and the Arduino RX pin (cathode/solid ring of diode connected at Arduino TX pin) to handle the half-duplex protocol over the single iBUS wire. See example wiring below. If you only have one sensor connected to the iBUS (i.e. only the Arduino board) you can replace the diode with a resistor of 1.2k Ohm.
- If you plan to **use both servo output and sensor data** in your sketch, your should use two different UART ports on your Arduino board.

For more information on the iBUS protocol, see (https://github.com/betaflight/betaflight/wiki/Single-wire-FlySky-(IBus)-telemetry). (please skip over the first part of the article how to combine the servo output and sensor data pins of the TGY receiver as it is more reliable to use two different UART ports on your Arduino if you need both servo and sensor data in your sketch).

### Additional telemetry sensors
IBusBM.h defines sensor types 
````
#define IBUSS_INTV 0 // Internal voltage (in 0.01)
#define IBUSS_TEMP 1 // Temperature (in 0.1 degrees, where 0=-40'C)
#define IBUSS_RPM  2 // RPM
#define IBUSS_EXTV 3 // External voltage (in 0.01)
````
If you have an OpenTx transceiver you can use additional telemetry sensors as defined in https://github.com/cleanflight/cleanflight/blob/7cd417959b3cb605aa574fc8c0f16759943527ef/src/main/telemetry/ibus_shared.h. Most sensors use 2 bytes of data. However, some sensors need 4 bytes of data. You can add the number of data bytes as second argument of the addSensor method as follows

````
#define IBUS_SENSOR_TYPE_GPS_LAT  0x80
IBus.addSensor(IBUS_SENSOR_TYPE_GPS_LAT, 4); 
````

### Example wiring

![Wiring with MEGA 2560](wiring.png?raw=true "Wiring with MEGA 2560")

Note: If no other sensors are connected to the iBUS you have the option to replace the diode with an 1.2k Ohm resistor.

### Example code for servo output only (AVR)

This example is for any AVR Arduino board. Note: this example is for AVR based boards only as the esp32 library does not support the analogwrite() function used by the servo.h library.

```
#include <IBusBM.h>
#include <Servo.h>

IBusBM IBus;    // IBus object
Servo myservo;  // create servo object to control a servo

void setup() {
  IBus.begin(Serial);    // iBUS object connected to serial0 RX pin
  myservo.attach(9);     // attaches the servo on pin 9 to the servo object
}

void loop() {
  int val;
  val = IBus.readChannel(0); // get latest value for servo channel 1
  myservo.writeMicroseconds(val);   // sets the servo position 
  delay(20);
}

```

You can support more servos by simply adding more Servo objects:
```
#include <IBusBM.h>
#include <Servo.h>

IBusBM IBus;    // IBus object
Servo myservo1;  // create servo object to control a servo
Servo myservo2;  // create servo object to control a servo

void setup() {
  IBus.begin(Serial);    // iBUS object connected to serial0 RX pin
  myservo1.attach(8);     // attaches the servo on pin 8 to the servo1 object
  myservo2.attach(9);     // attaches the servo on pin 9 to the servo2 object
}

void loop() {
  int val;
  val = IBus.readChannel(0); // get latest value for servo channel 1
  myservo1.writeMicroseconds(val);   // sets the servo position 
  val = IBus.readChannel(1); // get latest value for servo channel 2
  myservo2.writeMicroseconds(val);   // sets the servo position 
  delay(20);
}

```

### Example code for servo output on ESP32

This example is for any ESP32 board and is based on the Esp32Servo library (https://github.com/madhephaestus/ESP32Servo) that can be downloaded using the Arduino library manager.

```
#include <IBusBM.h>
#include <ESP32Servo.h>

IBusBM IBus;    // IBus object
Servo myservo;  // create servo object to control a servo

// Possible PWM GPIO pins on the ESP32: 0(used by on-board button),2,4,5(used by on-board LED),12-19,21-23,25-27,32-33 
#define servoPin 18
#define IBUS_RX_PIN 16
#define IBUS_TX_PIN 17

void setup() {
  IBus.begin(Serial2, 1, IBUS_RX_PIN, IBUS_TX_PIN); // iBUS object connected to Serial2
  myservo.attach(servoPin);     // attaches the servo on pin 18 to the servo object (using timer 0)
}

void loop() {
  int val;
  val = IBus.readChannel(0); // get latest value for servo channel 1
  myservo.writeMicroseconds(val);   // sets the servo position 
  delay(20);
}

```


### Example code for servo output on ESP32 with disabled timer

This example is for any ESP32 board and is based on the Esp32Servo library. In some cases you may need the ESP32 timers for other functions and you want to call the internal loop() function from your own code.

```
#include <IBusBM.h>
#include <ESP32Servo.h>

IBusBM IBus;    // IBus object
Servo myservo;  // create servo object to control a servo

// Possible PWM GPIO pins on the ESP32: 0(used by on-board button),2,4,5(used by on-board LED),12-19,21-23,25-27,32-33 
#define servoPin 18
#define IBUS_RX_PIN 16
#define IBUS_TX_PIN 17

void setup() {
  IBus.begin(Serial2, IBUSBM_NOTIMER, IBUS_RX_PIN, IBUS_TX_PIN);  // iBUS object connected to Serial2 using no timer
  myservo.attach(servoPin);     // attaches the servo on pin 18 to the servo object (using timer 0)
}

void loop() {
  int val;
  IBus.loop(); // call internal loop function to update the communication to the receiver 
  val = IBus.readChannel(0); // get latest value for servo channel 1
  myservo.writeMicroseconds(val);   // sets the servo position 
  delay(20);
}

```


### Example code for combined servo output and sensor input using two UART ports
This example is for the MEGA 2560 and ESP32 boards and will display servo debug information on the screen. 
No actual code to control a servo control is included in this example (screen display only).

```
#include <IBusBM.h>

IBusBM IBusServo;
IBusBM IBusSensor;

#if defined(ARDUINO_ARCH_ESP32)
#define IBUS_SERVO_RX_PIN 26
#define IBUS_SERVO_TX_PIN 27
#define IBUS_SENSOR_RX_PIN 16
#define IBUS_SENSOR_TX_PIN 17
#endif

#define TEMPBASE 400    // base value for temperature is -40'C

// sensor values
uint16_t speed=0;
uint16_t temp=TEMPBASE+200; // start at 20'C

void setup() {
  // initialize serial 0 port for debug messages on your PC
  Serial.begin(115200);

  // iBUS servo signal from receiver connected to RX of serial1 port
#if defined(ARDUINO_ARCH_ESP32)
  IBusServo.begin(Serial1, 0, IBUS_SERVO_RX_PIN, IBUS_SERVO_TX_PIN);
#else
  IBusServo.begin(Serial1);
#endif

  // iBUS sensor signal pins from receiver connected to RX + TX of serial2 port
  // (1N4148 diode included in TX line - cathode connected to TX)
#if defined(ARDUINO_ARCH_ESP32)
  IBusSensor.begin(Serial2, 0, IBUS_SENSOR_RX_PIN, IBUS_SENSOR_TX_PIN);
#else
  IBusSensor.begin(Serial2);
#endif
  
  Serial.println("Start iBUS");

  // adding 2 sensors to generate some dummy data
  IBusSensor.addSensor(IBUSS_RPM);
  IBusSensor.addSensor(IBUSS_TEMP);
}

void loop() {
  // show first 8 servo channels
  for (int i=0; i<8 ; i++) {
    Serial.print(IBusServo.readChannel(i), HEX);
    Serial.print(" ");
  }
  Serial.print(" ServoCnt=");
  Serial.print(IBusServo.cnt_rec); // count of how many times servo values have been updated
  Serial.print(" SensorCnt=");
  Serial.println(IBusSensor.cnt_sensor); // count of how many times sensor values have been sent back

  IBusSensor.setSensorMeasurement(1,speed);
  speed +=10;                           // increase motor speed by 10 RPM
  IBusSensor.setSensorMeasurement(2,temp++);  // increase temperature by 0.1 'C every loop

  delay(500);
}

```

## Class member functions and data members

The IBusBM class exposes the following functions:

```
- void begin(HardwareSerial& serial, int8_t timerid=0, int8_t rxPin=-1, int8_t txPin=-1);
- void begin(HardwareSerial& serial, int8_t timerid, int8_t rxPin, int8_t txPin, size_t rxBufferSize, size_t txBufferSize, uint8_t rxFifoFullThreshold);
```
This initializes the library for a given serial port. On ESP32, pass rxPin and txPin explicitly when you need stable wiring across Arduino ESP32 core versions and SoCs. Arduino ESP32 core 3.x changed several default UART pins, so relying on legacy RX1/TX1/RX2/TX2 defaults is not recommended.

On ESP32 Arduino core 3.x, IBusBM configures the UART through the HardwareSerial API: setPins(), setRxBufferSize(), setTxBufferSize(), and the native begin() RX FIFO threshold argument. The default ESP32 settings are a 256 byte RX buffer, a 64 byte TX buffer, and an RX FIFO full threshold of 1 byte. The extended begin() overload lets you override these values; rxFifoFullThreshold is clamped to the 1..127 range expected by the ESP32 UART driver. On non-ESP32 boards these extra buffer arguments are ignored.

The variable timerid specifies the ESP32 hardware timer only with Arduino ESP32 core 2.x. With core 3.x, the core allocates the timer from timerBegin(frequency), so timerid is kept for source compatibility. A value of IBUSBM_NOTIMER still disables the timer interrupt and you should call loop() yourself.

```
uint8_t addSensor(uint8_t type, uint8_t len=2); 
```
Defines new sensor of type "type", returns sensor number (first is number 1). The optional parameter "len" is the number of bytes used for storing the sensor value (can be 2 or 4).

```
uint16_t readChannel(uint8_t channelNr);
```
Read the value of servo channel 0..9 corresponding with servo channels 1..10.

```
void setSensorMeasurement(uint8_t adr, int32_t value);
```
Set value of sensor number adr to a given value (first sensor is number 1). The background process will send the value back through the receiver.

```
void loop();
```
Call the internal polling function (at least once per 1 ms) in case you disable the timer interrup. See below. If you have multiple instances of the IbusBM class, you only need to call this function for the last instance for which you call the begin() function. Example:

```
void setup() {
  .... other setup code goes here ....
  // code included in setup() for initiating the instances
  IBusServo.begin(Serial1, IBUSBM_NOTIMER);   // first instance
  IBusSensor.begin(Serial2, IBUSBM_NOTIMER);  // second instance
}

void loop() {
  .... other loop() code goes here ....
  // code to call the polling function. Should be called at least once every 1 ms
  IbusSensor.loop();  // IbusSensor.loop() will call IbusServo.loop(), no separate call is needed here.
  wait(1);
}
```

The IBusBM class exposes the following counters. Counters are 1 byte (value 0..255) and values can be used by your code to understand if new data is available. 

```
uint8_t cnt_rec; // count received number of servo messages from receiver
uint8_t cnt_poll; // count received number of sensor poll messages
uint8_t cnt_sensor; // count times a sensor value has been sent back
```

Counters can also be used to debug the hardware connections between the receiver and the Arduino board: If at least one sensor is defined, the RX pin will receive sensor poll messages. If the sensor is not able to "talk back" to the receiver the receiver will try to establish contact with the sensor every 7ms and the cnt_poll counter will increment. Only if the TX pin is correctly connected to the receiver, the cnt_sensor counter will increment and the cnt_poll value will stay the same.

## Failsafe

Failsafe defines what to do if the transmitter looses the connection with the receiver. You can not use the cnt_rec counter for failsafe as the receiver will continue to send servo data over the iBUS even if the connection with the transmitter is lost. You can implement failsafe by defining a failsafe value for a given servo channel in your transmitter (this only works for receivers that support failsafe mode). You can then read the channel value using readChannel() and test for the failsafe value.

## Sensor types

The following sensor types are defined in IBusBM.h:

```
#define IBUSS_INTV 0 // Internal voltage (in 0.01, so 500=5V)
#define IBUSS_TEMP 1 // Temperature (in 0.1 degrees Celcius, where 0=-40'C and 400=0 'C')
#define IBUSS_RPM  2 // RPM
#define IBUSS_EXTV 3 // External voltage (in 0.01, so 510=5.1V)
#define IBUS_PRESS 0x41 // Pressure (in Pa)
```
Depending on your transmitter you can use other sensor types too. If you have an OpenTX compatible transmitter, see the OpenTX documentation for the required sensor ID to pass to the addSensor() function.

## Background processing and Interrupts

IBusBM runs in the background using interrupts to ensure your code does not interfere with the iBUS timing. Timer0 is used by the core libraries on all Arduino AVR based boards to keep track of time for commands line millis() and delay(). IBusBM defines an additional interrupt on Timer 0 (the compare interrupt - TIMER0_COMPA_vect) to trigger the main process. If you want to change or disable timer 0 you can call the internal polling function loop() from your own code.  

On ESP32 boards with Arduino ESP32 core 2.x, the library uses timer 0 by default. You can overrule the timer by adding a second argument to the begin() function. With Arduino ESP32 core 3.x, the timer API no longer accepts a numeric timer id; IBusBM now requests a 1 MHz timer and configures a 1000 us auto-reload alarm through the core 3.x API. In both versions, use IBUSBM_NOTIMER when you want to disable the timer and call loop() from your own code.

In order to disable the timer background function you can add the IBUSBM_NOTIMER to the begin() function. You then need to call the loop() function from your own code at least once every 1 ms. If you define more than one IbusBM objects, you only need to call the loop() function for the first object.

## Example sketches provided

Example sketches:

- **Ibus2PWM**: converts iBUS data to Servo PWM format: Reads data from first servo channel and translates this to PWM signal to send to a normal servo (AVR version)
- **Ibus2PWM_ESP32**: converts iBUS data to Servo PWM format: Reads data from first servo channel and translates this to PWM signal to send to a normal servo (ESP32 version)
- **Ibus_singlemonitor**: monitor/debugger for receivers with a single iBUS pin (providing both servo and sensor data such as the Flysky X6B and Flysky FS-iA8X). Prints out servo channels to the standard serial output (PC debug window) and simulates 2 sensors with random values sent back to transmitter (as long as your receiver supports this). Requires Arduino board with 2 or more hardware serial ports (such as MEGA 2560)
- **Ibus_multimonitor**: monitor/debugger for receivers with a two separate iBUS pins (one for servo data and one for sensor data, such as the TGY-IA6B). Prints out servo channels to the standard serial output (PC debug window) and simulates 2 sensors with random values sent back to transmitter. Requires Arduino board with 3 or more hardware serial ports (such as MEGA 2560)
- **IBus_sensor**: simulate two telemetry sensors and send values back over the iBUS to the receiver to be shown in the display of your transmitter
- **Ibus_diy_servo_STM32**: example for (large) diy DC servo's, like windshield wiper motors. Change the  P I D settings for servo response and tuning
- **Ibus2PWM_mbed**: example translate iBUS signal to servo for MBED (Arduino Nano 33 BLE)
- **Robotcar**: Example remote controlled car using the VNH3SP30 motor driver
