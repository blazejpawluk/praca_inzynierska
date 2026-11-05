// Błażej Pawluk
// obsługa serwomechanizmu

#include "serwo.h"
#include <Servo.h>

// konfiguracja serwo
const uint8_t SERVO_PIN = 9;
const int CENTER_US = 1500;
const int MAX_ANGLE = 45;
const float US_PER_DEG = 1856.0 / 180.0;
int angle = 0; // względem środka: [-45,45]
Servo servo;

int angleToUs(int a) {
  return CENTER_US - (int)lround(a * US_PER_DEG);
}

void moveTo(int a) {
  angle = constrain(a, -MAX_ANGLE, MAX_ANGLE);
  servo.writeMicroseconds(angleToUs(angle));
}

void initServo() {
  servo.attach(SERVO_PIN);
  servo.writeMicroseconds(CENTER_US);
  delay(1500);
}

void setServoAngle(int a) {
  moveTo(90 - a);
}
