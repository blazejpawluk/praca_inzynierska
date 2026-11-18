/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Obsługa przerwania PCINT1 (zmiana stanu pinów A0-A5), używanego przez sonar

  Piny:
    - A2 (PCINT10) - ECHO sonaru
*/

#pragma once

void interruptsInit();