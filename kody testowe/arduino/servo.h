/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie serwomechanizmem SG90 z zakresem ruchu [0,180] stopni (używane [15,165] z powodu ograniczeń mechanicznych)

  Połączenie: D9 (sterowanie PWM)
*/

#pragma once

#include <Arduino.h>

// aktualny kąt serwa [stopnie] (tylko do odczytu poza servo.cpp)
extern int servoAngle;

void servoInit();
void servoSetAngle(int angle);