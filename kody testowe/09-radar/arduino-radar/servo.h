// Błażej Pawluk
// obsługa serwomechanizmu

#pragma once

#include <Arduino.h>
#include <Servo.h>

// pin serwo
const uint8_t SERVO_PIN = 9;

// konfiguracja serwo
const int CENTER_US = 1500;
const int MIN_US = 500; // domyślny zakres biblioteki Servo (544-2400 us) obcinałby skrajne kąty
const int MAX_US = 2500;
const int MAX_ANGLE = 80;
const float US_PER_DEG = 1856.0 / 180.0;
int angle = 0; // względem środka: [-90,90]
Servo servo;

void initServo() {
  servo.attach(SERVO_PIN, MIN_US, MAX_US);
  servo.writeMicroseconds(CENTER_US);
  delay(1500);
}

int angleToUs(int a) {
  return CENTER_US - (int)lround(a * US_PER_DEG);
}

void moveTo(int a) {
  angle = constrain(a, -MAX_ANGLE, MAX_ANGLE);
  servo.writeMicroseconds(angleToUs(angle));
}

void setServoAngle(int a) {
  moveTo(90 - a);
}