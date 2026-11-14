/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi sonaru HC-SR04 (opis w sonar.h)
*/

#include "sonar.h"
#include "servo.h"

// piny
const uint8_t TRIG_PIN = A3;
const uint8_t ECHO_PIN = A2;

// konfiguracja
const unsigned long TRIG_PULSE_US = 10; // długość impulsu wyzwalającego
const unsigned long ECHO_TIMEOUT_US = 25000; // czas echa dla MAX_VALID_MM
const unsigned long ECHO_START_DELAY_US = 1000; // zapas na opóźnienie HC-SR04 przed początkiem echa
const unsigned long ECHO_US_PER_MM_X100 = 583; // droga tam i z powrotem: 2 / 343 m/s = 5.83 us na mm
const unsigned int MIN_VALID_MM = 34;
const unsigned int MAX_VALID_MM = 4000;

// stan pomiaru (zmieniany także w przerwaniu)
static volatile bool busy = false; // pomiar w toku
static volatile bool echoHigh = false; // zarejestrowano początek echa w bieżącym pomiarze
static volatile bool echoReady = false; // zarejestrowano koniec echa w bieżącym pomiarze
static volatile unsigned long echoStart = 0;
static volatile unsigned long echoUs = 0;
static unsigned long trigUs = 0;
static int trigAngle = 0; // kąt serwa w chwili wyzwolenia pomiaru

void sonarInit() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
}

void sonarStart() {
  if (busy) return;

  noInterrupts();
  echoHigh = false;
  echoReady = false;
  busy = true;
  interrupts();
  trigAngle = servoAngle;

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(TRIG_PULSE_US);
  digitalWrite(TRIG_PIN, LOW);
  trigUs = micros();
}

bool sonarPoll(unsigned int& mm, int& angle) {
  if (!busy) return false;

  if (echoReady) {
    noInterrupts();
    unsigned long us = echoUs;
    busy = false;
    interrupts();

    mm = (us * 100UL) / ECHO_US_PER_MM_X100;
    if (mm < MIN_VALID_MM || mm > MAX_VALID_MM) mm = 0;
    angle = trigAngle;
    return true;
  }

  // brak echa w czasie odpowiadającym MAX_VALID_MM
  if (micros() - trigUs > ECHO_TIMEOUT_US + ECHO_START_DELAY_US) {
    busy = false;
    mm = 0;
    angle = trigAngle;
    return true;
  }
  return false;
}

bool sonarIsBusy() {
  return busy;
}

void sonarCancel() {
  busy = false;
}

void sonarISR(uint8_t state, uint8_t changed, unsigned long now) {
  if (!(changed & ECHO_BIT) || !busy) return; // zbocza poza pomiarem są ignorowane

  if (state & ECHO_BIT) {
    echoStart = now;
    echoHigh = true;
  } else if (echoHigh) {
    echoUs = now - echoStart;
    echoReady = true;
  }
}