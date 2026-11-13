/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie serwomechanizmem SG90 z zakresem ruchu [0,180] stopni (używane [15,165] z powodu ograniczeń mechanicznych)

  Połączenie: D9 (sterowanie PWM)
*/

#pragma once

#include <Arduino.h>
#include <Servo.h>

// pin
const uint8_t SERVO = 9;

// konfiguracja
const int MIN_US = 500;
const int CENTER_US = 1500;
const int MAX_US = 2500;
const long US_PER_180_DEG = 1856; // zmiana szerokości impulsu na pełne 180 stopni
int servoAngle; // [15,165]
Servo servo;

// obliczenia całkowite (bez float); zaokrąglenie w dół daje błąd < 1 us
int angleToUs(int a) {
  return CENTER_US + (int)((a - 90) * US_PER_180_DEG / 180);
}

void setServoAngle(int a) {
  servoAngle = a;
  servo.writeMicroseconds(angleToUs(a));
}

void initServo() {
  servo.attach(SERVO, MIN_US, MAX_US);
  setServoAngle(90);
  delay(500);
}