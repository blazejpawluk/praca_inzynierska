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

// wywołać po encodersInit() i sonarInit()
void interruptsInit();
