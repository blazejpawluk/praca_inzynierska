// Błażej Pawluk
// obsługa silników DC przez mostek H L298N

#pragma once
#include <Arduino.h>

extern int speedL, speedR; // aktualne prędkości: [-255,255], ujemne - jazda do tyłu

void initMotors();
void setMotors(int l, int r);         // ustawienie prędkości obu kół [-255,255]
void stopMotors();
void updateMotors(unsigned long now); // zatrzymanie silników po braku poleceń przez MOTOR_TIMEOUT
