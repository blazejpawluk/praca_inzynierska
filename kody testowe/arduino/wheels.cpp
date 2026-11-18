/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja sterowania silnikami kół (opis w wheels.h)
*/

#include "wheels.h"

// piny L298N
const uint8_t ENA_PIN = 5;
const uint8_t IN1_PIN = 4;
const uint8_t IN2_PIN = 3;
const uint8_t ENB_PIN = 6;
const uint8_t IN3_PIN = 7;
const uint8_t IN4_PIN = 8;

int motorSpeedL = 0;
int motorSpeedR = 0;

static void setSide(uint8_t en, uint8_t inA, uint8_t inB, int speed) {
  digitalWrite(inA, speed > 0 ? HIGH : LOW);
  digitalWrite(inB, speed < 0 ? HIGH : LOW);
  analogWrite(en, abs(speed));
}

// ujemna prędkość = jazda do tyłu
void motorsSet(int left, int right) {
  motorSpeedL = left;
  motorSpeedR = right;

  setSide(ENB_PIN, IN4_PIN, IN3_PIN, motorSpeedL);
  setSide(ENA_PIN, IN2_PIN, IN1_PIN, motorSpeedR);
}

void motorsInit() {
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  motorsSet(0, 0);
}