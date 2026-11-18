/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca enkodery szczelinowe kół (po jednym na przednie koło)

  Połączenie:
    - lewy enkoder: A0
    - prawy enkoder: A1

  Piny są próbkowane co 250 us w przerwaniu Timer2, dlatego oscylacje sygnału nie zwiększają
  obciążenia procesora. Timer2 jest zajęty, więc na pinach D3 i D11 nie działa analogWrite()
*/

#pragma once

#include <Arduino.h>

void encodersInit();

// liczba impulsów od uruchomienia (odczyt atomowy); impulsem jest każde zbocze, więc na szczelinę przypadają 2
void encodersRead(unsigned long& left, unsigned long& right);