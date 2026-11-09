// Błażej Pawluk
// komunikacja przez UART z ESP32-CAM

#pragma once
#include <Arduino.h>
#include "lcd.h"
#include "encoders.h"
#include "sonar.h"
#include "servo.h"
#include "mpu.h"
#include "wheels.h"
#include "position.h"

// dane
struct Data {
  unsigned long enL, enR;
  float sonar;
  float ax, ay, az;
  float gx, gy, gz;
  int mL, mR;
};

// odbieranie danych
const uint8_t RX_BUF_SIZE = 64;
char rxBuf[RX_BUF_SIZE];
uint8_t rxLen;

void handleLine(const char* line) {
  showOnLCD(line);

  if (strncmp(line, "C:S,", 4) == 0) {
    setServoAngle(atoi(line + 4));
  } else if (strcmp(line, "C:TRIG") == 0) {
    triggerSonar();
  } else if (strncmp(line, "C:M,", 4) == 0) {
    const char* comma = strchr(line + 4, ',');
    if (comma) setMotors(atoi(line + 4), atoi(comma + 1));
  } else if (strcmp(line, "C:STOP") == 0) {
    stopMotors();
  } else if (strcmp(line, "C:RESETPOS") == 0) {
    resetPosition();
  }
}

void readSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      if (rxLen > 0) handleLine(rxBuf);
      rxLen = 0;
    } else if (rxLen < RX_BUF_SIZE - 1) {
      rxBuf[rxLen++] = c;
    }
  }
}

void sendData() {
  Data d;

  // zbieranie danych z sensorów
  readEncoders(d.enL, d.enR);
  d.sonar = sonarMm / 10.0;
  d.ax = ax; d.ay = ay; d.az = az;
  d.gx = gx; d.gy = gy; d.gz = gz;
  d.mL = speedL; d.mR = speedR;

  // wysyłanie danych
  Serial.print(F("D:"));
  Serial.print(d.enL); Serial.print(',');
  Serial.print(d.enR); Serial.print(',');
  Serial.print(d.sonar, 1); Serial.print(',');
  Serial.print(d.ax, 2); Serial.print(',');
  Serial.print(d.ay, 2); Serial.print(',');
  Serial.print(d.az, 2); Serial.print(',');
  Serial.print(d.gx, 2); Serial.print(',');
  Serial.print(d.gy, 2); Serial.print(',');
  Serial.print(d.gz, 2); Serial.print(',');
  Serial.print(d.mL); Serial.print(',');
  Serial.println(d.mR);

  // wysyłanie pozycji
  Serial.print(F("P:"));
  Serial.print(posX * 100, 1); Serial.print(',');
  Serial.print(posY * 100, 1); Serial.print(',');
  Serial.print(heading, 1); Serial.print(',');
  Serial.println(vel * 100, 1);
}