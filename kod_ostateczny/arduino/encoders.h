/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca enkodery szczelinowe kół (po jednym na przednie koło)

  Połączenie:
    - lewy enkoder: A0
    - prawy enkoder: A1

  Przerwanie PCINT1 (wspólne z sonarem) jest w interrupts.h
*/

#pragma once

#include <Arduino.h>

// piny
const uint8_t ENL = A0;
const uint8_t ENR = A1;
const uint8_t ENL_BIT = 0b001; // PC0 w rejestrze PINC
const uint8_t ENR_BIT = 0b010; // PC1 w rejestrze PINC

// konfiguracja
const unsigned long DEBOUNCE_US = 2000;
volatile unsigned long enL = 0;
volatile unsigned long enR = 0;
volatile unsigned long lastL = 0;
volatile unsigned long lastR = 0;

void initEncoders() {
  pinMode(ENL, INPUT);
  pinMode(ENR, INPUT);
}

void readEncoders(unsigned long& l, unsigned long& r) {
  noInterrupts();
  l = enL;
  r = enR;
  interrupts();
}

void encodersISR(uint8_t rising, unsigned long now) {
  if ((rising & ENL_BIT) && (now - lastL > DEBOUNCE_US)) {
    enL++;
    lastL = now;
  }
  if ((rising & ENR_BIT) && (now - lastR > DEBOUNCE_US)) {
    enR++;
    lastR = now;
  }
}
