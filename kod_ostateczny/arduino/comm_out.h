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

// wiadomość jest wysyłana dopiero, gdy w buforze nadawczym (64 B, wolne max 63 B) jest co najmniej tyle miejsca;
// najdłuższa wiadomość (A:MPU) ma max 56 B, więc Serial.print nigdy nie czeka na wolne miejsce
const uint8_t TX_FREE_MIN = 60;

// cykl wysyłania: jedna wiadomość na obieg loop() w kolejności ENC, DIST, MPU, POS
enum SendPhase : uint8_t { SEND_IDLE, SEND_ENC, SEND_DIST, SEND_MPU, SEND_POS };
SendPhase sendPhase = SEND_IDLE;

// wynik pomiaru sonaru czekający na wysłanie
bool distPending = false;
int distAngle = 0;
unsigned int distMm = 0;

void sendEnc() {
  unsigned long l, r; readEncoders(l, r);

  Serial.print("A:ENC:");
  Serial.print(l);
  Serial.print(",");
  Serial.println(r);
}

void sendDist(int angle, unsigned int mm) {
  Serial.print("A:DIST:");
  Serial.print(angle);
  Serial.print(",");
  Serial.println(mm);
}

void sendMPU() {
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;
  float temp = 0;
  if (!readMPU(ax, ay, az, gx, gy, gz, temp)) return;

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

// zapamiętuje wynik pomiaru do wysłania; kąt z chwili wyzwolenia, a nie bieżący
// (serwo mogło się w tym czasie przestawić)
void queueDist(unsigned int mm) {
  distAngle = sonarAngle;
  distMm = mm;
  distPending = true;
}

// rozpoczyna nowy cykl wysyłania (co SEND_INTERVAL); pomija, jeśli poprzedni jeszcze trwa
void startSendCycle() {
  if (sendPhase != SEND_IDLE) return;
  if (!radarOn) startSonar(); // wynik trafi do queueDist() i zostanie wysłany w fazie DIST
  sendPhase = SEND_ENC;
}

// porzuca niewysłane wiadomości (np. po utracie połączenia)
void resetSendCycle() {
  sendPhase = SEND_IDLE;
  distPending = false;
}

// wysyła najwyżej jedną wiadomość; wywoływane w każdym obiegu loop()
void sendNext() {
  if (Serial.availableForWrite() < TX_FREE_MIN) return;

  // pomiar radaru jest wysyłany od razu; pomiar z cyklu dopiero w fazie DIST (po ENC)
  if (distPending && (radarOn || sendPhase == SEND_IDLE || sendPhase == SEND_DIST)) {
    distPending = false;
    sendDist(distAngle, distMm);
    if (sendPhase == SEND_DIST) sendPhase = SEND_MPU;
    return;
  }

  switch (sendPhase) {
    case SEND_ENC:
      sendEnc();
      sendPhase = SEND_DIST;
      break;
    case SEND_DIST:
      // czekamy na wynik pomiaru z cyklu; w trybie radaru DIST wysyła radar, więc pomijamy
      if (radarOn || !sonarBusy) sendPhase = SEND_MPU;
      break;
    case SEND_MPU:
      sendMPU();
      sendPhase = SEND_POS;
      break;
    case SEND_POS:
      sendPos();
      sendPhase = SEND_IDLE;
      break;
    case SEND_IDLE:
      break;
  }
}

void sendConnection() {
  Serial.println("A:CONNECTION");
}