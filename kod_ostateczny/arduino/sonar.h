/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie sonarem ultradźwiękowym HC-SR04

  Połączenie: TRIG=A3, ECHO=A2
*/

#pragma once

#include <Arduino.h>

// piny
const uint8_t TRIG = A3;
const uint8_t ECHO = A2;

// konfiguracja
const unsigned long ECHO_TIMEOUT_US = 25000;
const unsigned int MIN_VALID_MM = 34;
const unsigned int MAX_VALID_MM = 4000;

void initSonar() {
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  digitalWrite(TRIG, LOW);
}

unsigned int readSonar() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  unsigned long us = pulseIn(ECHO, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return 0;

  unsigned int mm = (us * 100UL) / 583UL;
  if (mm < MIN_VALID_MM || mm > MAX_VALID_MM) return 0;
  return mm;
}