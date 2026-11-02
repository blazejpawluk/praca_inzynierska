// Błażej Pawluk

#include <Arduino.h>
#include "Wheels.h"

#define SET_MOVEMENT(side, s1, s2) digitalWrite(side[0], s1); digitalWrite(side[1], s2);

Wheels::Wheels(uint8_t ENA, uint8_t ENB, uint8_t IN1, uint8_t IN2, uint8_t IN3, uint8_t IN4) {
  pinMode(ENA, OUTPUT); this->pinsL[2] = ENA;
  pinMode(IN1, OUTPUT); this->pinsL[0] = IN1;
  pinMode(IN2, OUTPUT); this->pinsL[1] = IN2;

  pinMode(ENB, OUTPUT); this->pinsR[2] = ENB;
  pinMode(IN3, OUTPUT); this->pinsR[0] = IN3;
  pinMode(IN4, OUTPUT); this->pinsR[1] = IN4;

  setSpeed(255);
}

void Wheels::setSpeedL(uint8_t s) {
  analogWrite(this->pinsL[2], s);
  this->speedL = s;
}

void Wheels::setSpeedR(uint8_t s) {
  analogWrite(this->pinsR[2], s);
  this->speedR = s;
}

void Wheels::setSpeed(uint8_t s) {
  setSpeedL(s);
  setSpeedR(s);
}

void Wheels::forwardL() {
  SET_MOVEMENT(pinsL, HIGH, LOW);
}

void Wheels::forwardR() {
  SET_MOVEMENT(pinsR, LOW, HIGH);
}

void Wheels::forward() {
  this->forwardL();
  this->forwardR();
}

void Wheels::backL() {
  SET_MOVEMENT(pinsL, LOW, HIGH);
}

void Wheels::backR() {
  SET_MOVEMENT(pinsR, HIGH, LOW);
}

void Wheels::back() {
  this->backL();
  this->backR();
}

void Wheels::stopL() {
  SET_MOVEMENT(pinsL, LOW, LOW);
}

void Wheels::stopR() {
  SET_MOVEMENT(pinsR, LOW, LOW);
}

void Wheels::stop() {
  this->stopL();
  this->stopR();
}