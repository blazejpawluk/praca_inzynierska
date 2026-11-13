/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca i sterująca radarem złożonym z serwomechanizmu SG90 i sonaru ultradźwiękowego HC-SR04
*/

#pragma once

#include <Arduino.h>
#include "servo.h"
#include "sonar.h"

// konfiguracja
bool radarOn;
int radarFrom, radarTo, radarStep;
unsigned long radarInterval;
extern int servoAngle;
int8_t radarDir = 1;
unsigned long lastRadarMs;
bool radarWaiting = false; // czekamy na wynik pomiaru przed ruchem serwa

void startRadar(int from, int to, int step, long interval) {
  radarFrom = from;
  radarTo = to;
  radarStep = step;
  radarInterval = interval;
  radarDir = 1;
  radarWaiting = false;

  setServoAngle(radarFrom);
  radarOn = true;

  lastRadarMs = millis();
}

void stopRadar() {
  radarOn = false;
  radarWaiting = false;
}

// po upływie radarInterval rozpoczyna pomiar; wynik odbiera loop() przez pollSonar()
void updateRadar() {
  if (!radarOn || radarWaiting) return;
  if (millis() - lastRadarMs < radarInterval) return;
  if (sonarBusy) return;

  startSonar();
  radarWaiting = true;
}

// wywoływane po otrzymaniu wyniku pomiaru: przestawia serwo na kolejny kąt
void radarNextStep() {
  radarWaiting = false;

  int next = servoAngle + radarDir * radarStep;
  if (radarDir > 0 && servoAngle >= radarTo) {
    radarDir = -1;
    next = servoAngle - radarStep;
  } else if (radarDir < 0 && servoAngle <= radarFrom) {
    radarDir = 1;
    next = servoAngle + radarStep;
  }
  // gdy (to - from) nie dzieli się przez step, ostatni krok kończy się dokładnie na granicy zakresu
  next = constrain(next, radarFrom, radarTo);
  setServoAngle(next);

  lastRadarMs = millis();
}