/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi enkoderów (opis w encoders.h)
*/

#include "encoders.h"

// piny
const uint8_t ENC_L_PIN = A0;
const uint8_t ENC_R_PIN = A1;

// konfiguracja
const unsigned long DEBOUNCE_US = 2000; // minimalny odstęp między impulsami (odrzuca drgania styku)

// liczniki impulsów (zmieniane w przerwaniu)
static volatile unsigned long pulsesL = 0;
static volatile unsigned long pulsesR = 0;

void encodersInit() {
  pinMode(ENC_L_PIN, INPUT);
  pinMode(ENC_R_PIN, INPUT);
}

void encodersRead(unsigned long& left, unsigned long& right) {
  noInterrupts();
  left = pulsesL;
  right = pulsesR;
  interrupts();
}

void encodersISR(uint8_t changed, unsigned long now) {
  static unsigned long lastL = 0;
  static unsigned long lastR = 0;

  if ((changed & ENC_L_BIT) && (now - lastL > DEBOUNCE_US)) {
    pulsesL++;
    lastL = now;
  }
  if ((changed & ENC_R_BIT) && (now - lastR > DEBOUNCE_US)) {
    pulsesR++;
    lastR = now;
  }
}
