/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca i sterująca radarem złożonym z serwomechanizmu SG90 i sonaru ultradźwiękowego HC-SR04
*/

#pragma once

#include <Arduino.h>
#include "servo.h"
#include "sonar.h"
#include "comm_out.h"

// konfiguracja
bool radarOn;
int radarFrom, radarTo, radarStep;
unsigned long radarInterval;
extern int servoAngle;
int8_t radarDir = 1;
unsigned long lastRadarMs;

void startRadar(int from, int to, int step, long interval) {
  radarFrom = from;
  radarTo = to;
  radarStep = step;
  radarInterval = interval;
  radarDir = 1;

  setServoAngle(radarFrom);
  radarOn = true;

  lastRadarMs = millis();
}

void stopRadar() {
  radarOn = false;
}

bool updateRadar() {
  if (!radarOn) return false;

  unsigned long now = millis();
  if (now - lastRadarMs < radarInterval) return false;

  sendDist();

  int next = servoAngle + radarDir * radarStep;
  if (radarDir > 0 && servoAngle >= radarTo) {
    radarDir = -1;
    next = servoAngle - radarStep;
  } else if (radarDir < 0 && servoAngle <= radarFrom) {
    radarDir = 1;
    next = servoAngle + radarStep;
  }
  setServoAngle(next);

  lastRadarMs = millis();
  return true;
}