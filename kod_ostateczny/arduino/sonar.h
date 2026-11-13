/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie sonarem ultradźwiękowym HC-SR04

  Połączenie: TRIG=A3, ECHO=A2

  Pomiar nie blokuje programu: sonarStart() wyzwala pomiar, czas trwania echa mierzy
  przerwanie PCINT1 (interrupts.cpp), a sonarPoll() zwraca wynik, gdy jest gotowy
*/

#pragma once

#include <Arduino.h>

// bit pinu ECHO w rejestrze PINC (potrzebny w interrupts.cpp)
const uint8_t ECHO_BIT = 0b100; // PC2 = A2

void sonarInit();

// rozpoczyna pomiar (blokuje tylko ~10 us); nic nie robi, jeśli pomiar jest w toku
void sonarStart();

// zwraca true, gdy pomiar się zakończył:
// mm – odległość (0 = brak echa lub poza zakresem), angle – kąt serwa w chwili wyzwolenia pomiaru
bool sonarPoll(unsigned int& mm, int& angle);

bool sonarIsBusy();

// porzuca trwający pomiar (spóźnione echo zostanie zignorowane)
void sonarCancel();

// wywoływane z przerwania PCINT1; state – stan pinów, changed – piny, które zmieniły stan
void sonarISR(uint8_t state, uint8_t changed, unsigned long now);
