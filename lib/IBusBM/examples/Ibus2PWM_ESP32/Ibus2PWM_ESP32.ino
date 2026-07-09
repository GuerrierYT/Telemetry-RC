#include <IBusBM.h>
#include <ESP32Servo.h>


/*
  Translate iBUS signal to servo - ESP32 version

  This example uses Esp32Servo library (https://github.com/madhephaestus/ESP32Servo) that can be
  downloaded using the Arduino library manager.
  
  Supports ESP32 boards with a free hardware UART.
  This example uses Serial2 with explicit pins so it works the same on ESP32 Arduino core 2.x and 3.x.

  Serial2 port RX/TX connected as follows:
  - RX connected to the iBUS servo pin 
  - TX can be left open as the iBUS protocol for servos is one way only

*/


IBusBM IBus;    // IBus object
Servo myservo;  // create servo object to control a servo

// Possible PWM GPIO pins on the ESP32: 0(used by on-board button),2,4,5(used by on-board LED),12-19,21-23,25-27,32-33 
#define servoPin 18
#define IBUS_RX_PIN 16
#define IBUS_TX_PIN 17

void setup() {
  Serial.begin(115200);     // debug info

  IBus.begin(Serial2, 1, IBUS_RX_PIN, IBUS_TX_PIN); // iBUS object connected to Serial2
  myservo.attach(servoPin); // attaches the servo on pin 18 to the servo object (using timer 0)
  Serial.println("Start IBus2PWM_ESP32");
}

int saveval=0;

void loop() {
  int val;
  val = IBus.readChannel(0); // get latest value for servo channel 1

  if (saveval != val) {
    Serial.println(val); // display new value in microseconds on PC
    saveval = val;    
    myservo.writeMicroseconds(val);   // sets the servo position 
  }
 
  delay(100);
}
