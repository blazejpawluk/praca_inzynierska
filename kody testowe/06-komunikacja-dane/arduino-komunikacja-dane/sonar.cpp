// Błażej Pawluk
// obsługa sonaru HC-SR04

#include "sonar.h"

// konfiguracja sonaru
const uint8_t TRIG_PIN = A3;
const uint8_t ECHO_PIN = A2;
const unsigned long ECHO_TIMEOUT_US = 25000;
const unsigned int MIN_VALID_MM = 34;
const unsigned int MAX_VALID_MM = 4000;

unsigned int sonarMm = 0;

// mierzy odległość do przeszkody sonarem w mm
unsigned int measureMm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long us = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return 0;

  // dźwięk ~343 m/s -> 1 mm w obie strony = ~5.83 us
  unsigned int mm = (us * 100UL) / 583UL;
  if (mm < MIN_VALID_MM || mm > MAX_VALID_MM) return 0;
  return mm;
}

void initSonar() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void triggerSonar() {
  sonarMm = measureMm();
}
