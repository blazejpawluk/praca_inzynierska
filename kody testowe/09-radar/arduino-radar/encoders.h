// Błażej Pawluk
// obsługa enkoderów kół

#pragma once

#include <Arduino.h>

// piny enkoderów
const uint8_t ENL_PIN = A0;
const uint8_t ENR_PIN = A1;

// konfiguracja enkoderów
const unsigned long DEBOUNCE_US = 2000;
volatile unsigned long enL = 0;
volatile unsigned long enR = 0;
volatile unsigned long lastL = 0;
volatile unsigned long lastR = 0;
volatile uint8_t prevState = 0;

void initEncoders() {
  pinMode(ENL_PIN, INPUT);
  pinMode(ENR_PIN, INPUT);

  // włączenie przerwań
  noInterrupts();
  prevState = PINC & 0b00000011;
  PCICR |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9);
  interrupts();
}

void readEncoders(unsigned long& l, unsigned long& r) {
  noInterrupts();
  l = enL;
  r = enR;
  interrupts();
}

ISR(PCINT1_vect) {
  uint8_t state = PINC & 0b00000011;
  uint8_t rising = state & ~prevState;
  prevState = state;
  unsigned long now = micros();

  if ((rising & 0b01) && (now - lastL > DEBOUNCE_US)) {
    enL++;
    lastL = now;
  }
  if ((rising & 0b10) && (now - lastR > DEBOUNCE_US)) {
    enR++;
    lastR = now;
  }
}