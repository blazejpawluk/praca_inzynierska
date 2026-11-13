/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Obsługa przerwania PCINT1 (zmiana stanu pinów A0-A5), wspólnego dla enkoderów i sonaru

  Piny:
    - A0 (PCINT8)  - lewy enkoder
    - A1 (PCINT9)  - prawy enkoder
    - A2 (PCINT10) - ECHO sonaru
*/

#pragma once

#include <Arduino.h>
#include "encoders.h"
#include "sonar.h"

const uint8_t PCINT1_PINS = ENL_BIT | ENR_BIT | ECHO_BIT;
volatile uint8_t prevState = 0;

// wywołać po initEncoders() i initSonar()
void initInterrupts() {
  noInterrupts();
  prevState = PINC & PCINT1_PINS;
  PCICR |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9) | (1 << PCINT10);
  interrupts();
}

ISR(PCINT1_vect) {
  uint8_t state = PINC & PCINT1_PINS;
  uint8_t changed = state ^ prevState;
  prevState = state;
  unsigned long now = micros();

  encodersISR(changed & state, now);
  sonarISR(state, changed, now);
}
