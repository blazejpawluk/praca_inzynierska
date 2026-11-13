/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca komunikację wychodzącą przez UART do ESP32-CAM

  Połączenie: UART TX=D1 (do ESP32-CAM przez konwerter poziomów logicznych z 5V na 3.3V)

  Protokoły komunikacji wychodzącej z Arduino::
    - A:ENC:<left>,<right> - enkodery kół [liczba impulsów]
    - A:DIST:<angle>,<distance> - odległość od przeszkody [stopnie, mm]
    - A:MPU:<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<temp> - MPU6050 [g, stopnie/s, stopnie celcjusza]
    - A:POS:<x>,<y>,<angle>,<velocity> - pozycja [mm, mm, stopnie, mm/s]
    - A:CONNECTION - żądanie statusu połączenia z komputerem
*/

#pragma once

#include <Arduino.h>
#include "encoders.h"
#include "sonar.h"
#include "mpu.h"
#include "position.h"

extern bool radarOn;
void readSerial();

void sendEnc() {
  unsigned long l, r; readEncoders(l, r);

  Serial.print("A:ENC:");
  Serial.print(l);
  Serial.print(",");
  Serial.println(r);
}

extern int servoAngle;
void sendDist(unsigned int mm) {
  Serial.print("A:DIST:");
  Serial.print(servoAngle);
  Serial.print(",");
  Serial.println(mm);
}

void sendMPU() {
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;
  float temp = 0;
  readMPU(ax, ay, az, gx, gy, gz, temp);

  Serial.print("A:MPU:");
  Serial.print(ax);
  Serial.print(",");
  Serial.print(ay);
  Serial.print(",");
  Serial.print(az);
  Serial.print(",");
  Serial.print(gx);
  Serial.print(",");
  Serial.print(gy);
  Serial.print(",");
  Serial.print(gz);
  Serial.print(",");
  Serial.println(temp);
}

void sendPos() {
  Serial.print("A:POS:");
  Serial.print(posX);
  Serial.print(",");
  Serial.print(posY);
  Serial.print(",");
  Serial.print(angle);
  Serial.print(",");
  Serial.println(velocity);
}

void sendData() {
  sendEnc();
  readSerial();
  if (!radarOn) startSonar(); // wynik wysyła loop() po zakończeniu pomiaru
  sendMPU();
  readSerial();
  sendPos();
}

void sendConnection() {
  Serial.println("A:CONNECTION");
}