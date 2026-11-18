/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi enkoderów (opis w encoders.h)
*/

#include "encoders.h"

// piny
const uint8_t ENC_L_PIN = A0;
const uint8_t ENC_R_PIN = A1;

// bity pinów w rejestrze PINC
const uint8_t ENC_L_BIT = 0b001; // PC0 = A0
const uint8_t ENC_R_BIT = 0b010; // PC1 = A1

// konfiguracja
const uint8_t SAMPLE_TIMER_TOP = 124; // 16 MHz / 32 / (124 + 1) = 4 kHz, czyli próbka co 250 us
const uint8_t STABLE_SAMPLES = 3; // tyle kolejnych próbek musi potwierdzić zmianę stanu

// liczniki impulsów (zmieniane w przerwaniu)
static volatile unsigned long pulsesL = 0;
static volatile unsigned long pulsesR = 0;

// stan filtru (używany tylko w przerwaniu po uruchomieniu timera)
static uint8_t stableState = 0;
static uint8_t changeCountL = 0;
static uint8_t changeCountR = 0;

void encodersInit() {
  pinMode(ENC_L_PIN, INPUT);
  pinMode(ENC_R_PIN, INPUT);

  noInterrupts();
  stableState = PINC & (ENC_L_BIT | ENC_R_BIT);

  // Timer2 w trybie CTC, preskaler 32
  TCCR2A = (1 << WGM21);
  TCCR2B = (1 << CS21) | (1 << CS20);
  TCNT2 = 0;
  OCR2A = SAMPLE_TIMER_TOP;
  TIFR2 = (1 << OCF2A);
  TIMSK2 = (1 << OCIE2A);
  interrupts();
}

void encodersRead(unsigned long& left, unsigned long& right) {
  noInterrupts();
  left = pulsesL;
  right = pulsesR;
  interrupts();
}

// zalicza impuls, gdy pin ma inny stan niż stabilny przez STABLE_SAMPLES kolejnych próbek
static inline void filterChannel(uint8_t sample, uint8_t bit, uint8_t& changeCount, volatile unsigned long& pulses) {
  if (!((sample ^ stableState) & bit)) {
    changeCount = 0;
    return;
  }

  if (++changeCount >= STABLE_SAMPLES) {
    changeCount = 0;
    stableState ^= bit;
    pulses++;
  }
}

ISR(TIMER2_COMPA_vect) {
  uint8_t sample = PINC;
  filterChannel(sample, ENC_L_BIT, changeCountL, pulsesL);
  filterChannel(sample, ENC_R_BIT, changeCountR, pulsesR);
}
