/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja sterowania serwomechanizmem SG90 (opis w servo.h)
*/

#include "servo.h"
#include <Servo.h>

// pin
const uint8_t SERVO_PIN = 9;

// konfiguracja
const int MIN_US = 500;
const int CENTER_US = 1500;
const int MAX_US = 2500;
const int SERVO_CENTER_DEG = 90;
const long US_PER_180_DEG = 1856;        // zmiana szerokości impulsu na pełne 180 stopni
const unsigned long SERVO_INIT_MS = 500; // czas na dojechanie do środka po starcie

int servoAngle = SERVO_CENTER_DEG;
static Servo servo;

// obliczenia całkowite (bez float); zaokrąglenie w dół daje błąd < 1 us
static int angleToUs(int angle) {
  return CENTER_US + (int)((angle - SERVO_CENTER_DEG) * US_PER_180_DEG / 180);
}

void servoSetAngle(int angle) {
  servoAngle = angle;
  servo.writeMicroseconds(angleToUs(angle));
}

void servoInit() {
  servo.attach(SERVO_PIN, MIN_US, MAX_US);
  servoSetAngle(SERVO_CENTER_DEG);
  delay(SERVO_INIT_MS);
}
