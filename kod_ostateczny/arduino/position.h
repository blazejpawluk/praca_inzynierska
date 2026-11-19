/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka śledząca aktualną pozycję pojazdu

  Korzysta z danych z enkoderów szczelinowych kół do wyznaczania przejechanej odległości
  oraz żyroskopu do wyznaczania kąta, o który obrócił się pojazd

  Impulsy koła są liczone tylko podczas pracy jego silnika i krótkiego wybiegu,
  a kąt tylko wtedy, gdy warunek ten spełnia co najmniej jedno koło
*/

#pragma once

// aktualny stan pojazdu (tylko do odczytu poza position.cpp)
extern float posX, posY; // pozycja [mm]
extern float heading; // kierunek jazdy [stopnie, (-180, 180]]
extern float velocity; // prędkość [mm/s]

// poza pojazdu
struct Pose {
  float x, y; // pozycja [mm]
  float heading; // kierunek jazdy [stopnie, (-180, 180]]
};

// zwraca aktualną pozę
Pose positionGet();

// zeruje bufory pomocnicze i czas ostatniej próbki (bez zmiany pozycji);
void positionInit();

// przywraca pozę zapisaną przed resetem mikrokontrolera; zwraca false po włączeniu zasilania
// (pamięć nie zawiera wtedy poprawnego zapisu) - poza pozostaje wtedy (0, 0, 0)
bool positionRestore();

// ustawia pozycję i kierunek na (0, 0, 0)
void positionReset();

// aktualizuje pozycję, jeśli MPU6050 ma nową próbkę
void positionPoll();