// Błażej Pawluk
// komunikacja z komputerem (UART > ESP32-CAM > WebSocket > strona internetowa)

#include "lcd.h"
#include "wheels.h"
#include "servo.h"
#include "encoders.h"
#include "sonar.h"
#include "mpu.h"
#include "position.h"
#include "radar.h"
#include "communication.h"

// konfiguracja UART
const unsigned long BAUD = 38400;
const unsigned long SEND_INTERVAL = 200;
unsigned long lastSend = 0;

void setup() {
  initLCD();
  showStatus("INITIALIZATION", "DON'T TOUCH!");
  delay(1000);

  showStatus("MOTORS", "DON'T TOUCH!");
  initMotors();

  showStatus("SERVO", "DON'T TOUCH!");
  initServo();

  showStatus("ENCODERS", "DON'T TOUCH!");
  initEncoders();

  showStatus("SONAR", "DON'T TOUCH!");
  initSonar();

  showStatus("MPU", "DON'T TOUCH!");
  if (initMPU()) {
    showStatus("CALIBRATION", "DON'T TOUCH!");
    calibrateGyro();
  } else {
    showStatus("MPU ERROR", "");
    delay(2000);
  }
  initPosition();

  Serial.begin(BAUD);
  showStatus("READY TO CONNECT", "waiting for data");

  lastSend = millis();
}

void loop() {
  unsigned long now = millis();
  
  readSerial();
  if (updateMPU(now)) updatePosition();
  if (updateRadar(now)) sendRadar();

  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    sendData();
  }
}