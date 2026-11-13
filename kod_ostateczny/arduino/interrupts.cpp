/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja przerwania PCINT1 (opis w interrupts.h)
*/

#include <Arduino.h>
#include "interrupts.h"
#include "encoders.h"
#include "sonar.h"

// bit n rejestrów PINC i PCMSK1 odpowiada pinowi PCn (A0 = PC0 = PCINT8 itd.)
const uint8_t PCINT1_PINS = ENC_L_BIT | ENC_R_BIT | ECHO_BIT;

static uint8_t prevState = 0; // stan pinów przy poprzednim przerwaniu

void interruptsInit() {
  noInterrupts();
  prevState = PINC & PCINT1_PINS;
  PCICR |= (1 << PCIE1);
  PCMSK1 |= PCINT1_PINS;
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
