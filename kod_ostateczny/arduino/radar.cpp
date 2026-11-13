/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja radaru (opis w radar.h)
*/

#include <Arduino.h>
#include "radar.h"
#include "servo.h"
#include "sonar.h"

bool radarOn = false;

static int radarFrom, radarTo, radarStep;
static unsigned long radarIntervalMs;
static int8_t radarDir = 1;
static unsigned long lastRadarMs = 0;
static bool radarWaiting = false; // czekamy na wynik pomiaru przed ruchem serwa

void radarStart(int from, int to, int step, long intervalMs) {
  radarFrom = from;
  radarTo = to;
  radarStep = step;
  radarIntervalMs = intervalMs;
  radarDir = 1;
  radarWaiting = false;

  servoSetAngle(radarFrom);
  radarOn = true;

  lastRadarMs = millis();
}

void radarStop() {
  radarOn = false;
  radarWaiting = false;
}

void radarPause() {
  radarWaiting = false;
}

void radarUpdate() {
  if (!radarOn || radarWaiting) return;
  if (millis() - lastRadarMs < radarIntervalMs) return;
  if (sonarIsBusy()) return; // pomiar z cyklu wysyłania mógł zacząć się przy poprzednim kącie – czekamy na własny

  sonarStart();
  radarWaiting = true;
}

void radarOnMeasurement() {
  if (!radarWaiting) return;
  radarWaiting = false;

  // zmiana kierunku na granicy zakresu
  if (radarDir > 0 && servoAngle >= radarTo) radarDir = -1;
  else if (radarDir < 0 && servoAngle <= radarFrom) radarDir = 1;

  // gdy (to - from) nie dzieli się przez step, ostatni krok kończy się dokładnie na granicy zakresu
  int next = servoAngle + radarDir * radarStep;
  next = constrain(next, radarFrom, radarTo);
  servoSetAngle(next);

  lastRadarMs = millis();
}
