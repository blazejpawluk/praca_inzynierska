// Błażej Pawluk
// tryb radaru

#pragma once

#include <Arduino.h>
#include "servo.h"
#include "sonar.h"

// konfiguracja radaru
const int MIN_RADAR_ANGLE = 15;
const int MAX_RADAR_ANGLE = 165;
const int MAX_RADAR_STEP = 15;
const unsigned long MIN_RADAR_INTERVAL = 30;
const unsigned long RADAR_START_MS = 500;

// ustawiane dane
bool radarOn = false;
int radarFrom = MIN_RADAR_ANGLE,
    radarTo = MAX_RADAR_ANGLE,
    radarStep = 5;
unsigned long radarInterval = 50;

// zbierane dane
int radarMeasurementAngle = MIN_RADAR_ANGLE; // kąt ostatniego pomiaru

// zmienne systemu
int radarAngle = MIN_RADAR_ANGLE; // obecny kąt serva
int8_t radarDir = 1;
unsigned long radarWait = 0;
unsgined long lastRadarMs = 0;

void startRadar(int from, int to, int step, long interval) {
  radarFrom = constrain(from, MIN_RADAR_ANGLE, MAX_RADAR_ANGLE);
  radarTo = constrain(to, MIN_RADAR_ANGLE, MAX_RADAR_ANGLE);
  radarStep = constrain(step, 1, MAX_RADAR_STEP);
  radarInterval = max(interval, (long)MIN_RADAR_INTERVAL);

  radarAngle = radarFrom;
  radarDir = 1;
  setServoAngle(radarAngle);
  radarWait = RADAR_START_MS;
  lastRadarMs = millis();
  radarOn = true;
}

void stopRadar() {
  radarOn = false;
}

bool updateRadar(unsigned long low) {
  if (!radarOn) return false;
  if (now - lastRadarMs < radarWait) return false;

  triggerSonar();
  radarMeasurementAngle = radarAngle;

  int next = radarAngle + radarDir * radarStep;
  if (radarDir > 0 && radarAngle >= radarTo) {
    radarDir = -1;
    next = radarAngle - radarStep;
  } else if (radarDir < 0 && radarAngle <= radarFrom) {
    radarDir = 1;
    next = radarAngle + radarStep;
  }
  radarAngle = constrain(next, radarFrom, radarTo);
  setServoAngle(radarAngle);

  radarWait = radarInterval;
  lastRadarMs = millis();
  return true;
}