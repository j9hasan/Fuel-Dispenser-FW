#include <SoftwareSerial.h>

SoftwareSerial rs485(10, 11); // RX, TX (TX not used)

void setup() {
  Serial.begin(9600);      // PC monitor
  rs485.begin(4800);       // RS485 baud
}

void loop() {
  if (rs485.available()) {
    char c = rs485.read();
    Serial.print(c);
  }
}
