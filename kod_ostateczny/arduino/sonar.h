/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie sonarem ultradźwiękowym HC-SR04

  Połączenie: TRIG=A3, ECHO=A2

  Pomiar nie blokuje programu: startSonar() wyzwala pomiar, czas trwania echa mierzy
  przerwanie PCINT1 (interrupts.h), a pollSonar() zwraca wynik, gdy jest gotowy
*/

#pragma once

#include <Arduino.h>

// piny
const uint8_t TRIG = A3;
const uint8_t ECHO = A2;
const uint8_t ECHO_BIT = 0b100; // PC2 w rejestrze PINC

// konfiguracja
const unsigned long ECHO_TIMEOUT_US = 25000;
const unsigned int MIN_VALID_MM = 34;
const unsigned int MAX_VALID_MM = 4000;

// stan pomiaru
volatile bool sonarBusy = false; // pomiar w toku
volatile bool echoHigh = false; // zarejestrowano początek echa w bieżącym pomiarze
volatile bool echoReady = false; // zarejestrowano koniec echa w bieżącym pomiarze
volatile unsigned long echoStart = 0;
volatile unsigned long echoUs = 0;
unsigned long trigUs = 0;
extern int servoAngle;
int sonarAngle = 0; // kąt serwa w chwili wyzwolenia pomiaru

void initSonar() {
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  digitalWrite(TRIG, LOW);
}

void startSonar() {
  if (sonarBusy) return;

  noInterrupts();
  echoHigh = false;
  echoReady = false;
  sonarBusy = true;
  interrupts();
  sonarAngle = servoAngle;

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  trigUs = micros();
}

bool pollSonar(unsigned int& mm) {
  if (!sonarBusy) return false;

  if (echoReady) {
    noInterrupts();
    unsigned long us = echoUs;
    sonarBusy = false;
    interrupts();

    mm = (us * 100UL) / 583UL;
    if (mm < MIN_VALID_MM || mm > MAX_VALID_MM) mm = 0;
    return true;
  }

  if (micros() - trigUs > ECHO_TIMEOUT_US + 1000) {
    sonarBusy = false;
    mm = 0;
    return true;
  }
  return false;
}

void sonarISR(uint8_t state, uint8_t changed, unsigned long now) {
  if (!(changed & ECHO_BIT) || !sonarBusy) return;

  if (state & ECHO_BIT) {
    echoStart = now;
    echoHigh = true;
  } else if (echoHigh) {
    echoUs = now - echoStart;
    echoReady = true;
  }
}
