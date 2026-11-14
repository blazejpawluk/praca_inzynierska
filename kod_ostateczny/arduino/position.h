/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka śledząca aktualną pozycję pojazdu

  Korzysta z danych z enkoderów szczelinowych kół do wyznaczania przejechanej odległości
  oraz żyroskopu do wyznaczania kąta, o który obrócił się pojazd
*/

#pragma once

// aktualny stan pojazdu (tylko do odczytu poza position.cpp)
extern float posX, posY; // pozycja [mm]
extern float heading; // kierunek jazdy [stopnie, (-180, 180]]
extern float velocity; // prędkość [mm/s]

// zeruje bufory pomocnicze i czas ostatniej próbki (bez zmiany pozycji);
void positionInit();

// ustawia pozycję i kierunek na (0, 0, 0)
void positionReset();

// aktualizuje pozycję, jeśli MPU6050 ma nową próbkę
void positionPoll();