/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca enkodery szczelinowe kół (po jednym na przednie koło)

  Połączenie:
    - lewy enkoder: A0
    - prawy enkoder: A1

  Przerwanie PCINT1 (wspólne z sonarem) jest w interrupts.cpp
*/

#pragma once

#include <Arduino.h>

// bity pinów enkoderów w rejestrze PINC (potrzebne w interrupts.cpp)
const uint8_t ENC_L_BIT = 0b001; // PC0 = A0
const uint8_t ENC_R_BIT = 0b010; // PC1 = A1

void encodersInit();

// liczba impulsów od uruchomienia (odczyt atomowy); impulsem jest każde zbocze, więc na szczelinę przypadają 2
void encodersRead(unsigned long& left, unsigned long& right);

// wywoływane z przerwania PCINT1; changed – bity pinów, które zmieniły stan
void encodersISR(uint8_t changed, unsigned long now);