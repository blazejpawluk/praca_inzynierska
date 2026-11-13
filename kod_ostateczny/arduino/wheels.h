/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie silnikami kół przez sterownik L298N

  Połączenie z L298N:
    - silnik prawy: ENA=5, IN1=4, IN2=3
    - silnik lewy: ENB=6, IN3=7, IN4=8
*/

#pragma once

#include <Arduino.h>

// aktualne prędkości [PWM, ujemna = jazda do tyłu] (tylko do odczytu poza wheels.cpp)
extern int motorSpeedL, motorSpeedR;

void motorsInit();
void motorsSet(int left, int right);
