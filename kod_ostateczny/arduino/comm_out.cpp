/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja komunikacji wychodzącej (opis i protokół w comm_out.h)
*/

#include <Arduino.h>
#include "comm_out.h"
#include "encoders.h"
#include "sonar.h"
#include "mpu.h"
#include "position.h"
#include "radar.h"

// wiadomość jest wysyłana dopiero, gdy w buforze nadawczym (64 B, wolne max 63 B) jest co najmniej tyle miejsca;
// najdłuższa wiadomość (A:MPU) ma max 52 B, więc Serial.print nigdy nie czeka na wolne miejsce
const uint8_t TX_FREE_MIN = 60;

const unsigned long CONNECTION_INTERVAL_MS = 100;
static unsigned long lastConnectionMs = 0;

// cykl wysyłania: jedna wiadomość na obieg loop() w kolejności ENC, DIST, MPU, POS
enum SendPhase : uint8_t { SEND_IDLE, SEND_ENC, SEND_DIST, SEND_MPU, SEND_POS };
static SendPhase sendPhase = SEND_IDLE;

// wynik pomiaru sonaru czekający na wysłanie
static bool distPending = false;
static int distAngle = 0;
static unsigned int distMm = 0;

static void sendEnc() {
  unsigned long left, right;
  encodersRead(left, right);

  Serial.print(F("A:ENC:"));
  Serial.print(left);
  Serial.print(',');
  Serial.println(right);
}

static void sendDist(int angle, unsigned int mm) {
  Serial.print(F("A:DIST:"));
  Serial.print(angle);
  Serial.print(',');
  Serial.println(mm);
}

static void sendMPU() {
  MpuData data;
  if (!mpuRead(data)) return;

  Serial.print(F("A:MPU:"));
  Serial.print(data.ax);
  Serial.print(',');
  Serial.print(data.ay);
  Serial.print(',');
  Serial.print(data.az);
  Serial.print(',');
  Serial.print(data.gx);
  Serial.print(',');
  Serial.print(data.gy);
  Serial.print(',');
  Serial.print(data.gz);
  Serial.print(',');
  Serial.println(data.temp);
}

static void sendPos() {
  // liczby całkowite zamiast float: Serial.print(float) dołącza ~450 B kodu formatowania
  Serial.print(F("A:POS:"));
  Serial.print(lround(posX));
  Serial.print(',');
  Serial.print(lround(posY));
  Serial.print(',');
  Serial.print(lround(heading * 100));
  Serial.print(',');
  Serial.println(lround(velocity));
}

void commQueueDist(int angle, unsigned int mm) {
  distAngle = angle;
  distMm = mm;
  distPending = true;
}

void commStartSendCycle() {
  if (sendPhase != SEND_IDLE) return;
  if (!radarOn) sonarStart(); // wynik trafi do commQueueDist() i zostanie wysłany w fazie DIST
  sendPhase = SEND_ENC;
}

void commResetSendCycle() {
  sendPhase = SEND_IDLE;
  distPending = false;
}

void commSendNext() {
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
      if (radarOn || !sonarIsBusy()) sendPhase = SEND_MPU;
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

void commRequestConnection() {
  if (millis() - lastConnectionMs < CONNECTION_INTERVAL_MS) return;
  Serial.println(F("A:CONNECTION"));
  lastConnectionMs = millis();
}
