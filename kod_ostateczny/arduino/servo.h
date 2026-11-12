/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie serwomechanizmem SG90 z zakresem ruchu [0,180] stopni

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
const float US_PER_DEG = 1856.0 / 180.0;
int servoAngle; // [15,165]
Servo servo;

int angleToUs(int a) {
  return 1500 + (int)lround((a - 90) * US_PER_DEG);
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