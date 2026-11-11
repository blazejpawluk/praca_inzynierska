// Błażej Pawluk
// tryb radaru (serwo + sonar)

#pragma once

#include <Arduino.h>
#include "servo.h"
#include "sonar.h"

// konfiguracja radaru
const int MIN_RADAR_ANGLE = 0;
const int MAX_RADAR_ANGLE = 180;
const int MAX_RADAR_STEP = 90;
const unsigned long MIN_RADAR_INTERVAL = 30; // czas na dojazd serwa i wygaśnięcie echa [ms]
const unsigned long RADAR_START_MS = 500; // dojazd serwa do kąta startowego [ms]

// ustawiane dane
bool radarOn = false;
int radarFrom = 0, radarTo = 180, radarStep = 2;
unsigned long radarInterval = 80;

// zbierane dane
int radarMeasAngle = 0; // kąt ostatniego pomiaru

// zmienne systemu
int radarAngle = 0; // aktualnie ustawiony kąt
int8_t radarDir = 1;
unsigned long radarWait = 0;
unsigned long lastRadarMs = 0;

void startRadar(int from, int to, int step, long interval) {
  from = constrain(from, MIN_RADAR_ANGLE, MAX_RADAR_ANGLE);
  to = constrain(to, MIN_RADAR_ANGLE, MAX_RADAR_ANGLE);
  if (from > to) { int t = from; from = to; to = t; }

  radarFrom = from;
  radarTo = to;
  radarStep = constrain(step, 1, MAX_RADAR_STEP);
  radarInterval = max(interval, (long)MIN_RADAR_INTERVAL);

  radarAngle = radarFrom;
  radarDir = 1;
  setServoAngle(radarAngle);
  radarWait = max(radarInterval, RADAR_START_MS);
  lastRadarMs = millis();
  radarOn = true;
}

void stopRadar() {
  radarOn = false;
}

// zwraca true, gdy jest nowy pomiar (radarMeasAngle, sonarMm)
bool updateRadar(unsigned long now) {
  if (!radarOn) return false;
  if (now - lastRadarMs < radarWait) return false;

  // pomiar na aktualnym kącie
  triggerSonar();
  radarMeasAngle = radarAngle;

  // kolejny kąt (ruch tam i z powrotem, skrajne kąty zawsze mierzone)
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
