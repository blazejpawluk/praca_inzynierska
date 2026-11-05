// Błażej Pawluk
// obsługa serwomechanizmu

#pragma once
#include <Arduino.h>

void initServo();
void setServoAngle(int a); // kąt w zakresie [45,135], 90 - na wprost
