// Błażej Pawluk
// obsługa silników kół

#pragma once

#include <Arduino.h>

// piny silników
const uint8_t ENA_PIN = 3;
const uint8_t IN1_PIN = 4;
const uint8_t IN2_PIN = 5;
const uint8_t ENB_PIN = 6;
const uint8_t IN3_PIN = 7;
const uint8_t IN4_PIN = 8;

// konfiguracja silników
const int MAX_SPEED = 255;

// ustawiane dane
int speedL = 0, speedR = 0;

void setSide(uint8_t en, uint8_t inA, uint8_t inB, int s) {
  digitalWrite(inA, s > 0 ? HIGH : LOW);
  digitalWrite(inB, s < 0 ? HIGH : LOW);
  analogWrite(en, abs(s));
}

void setMotors(int l, int r) {
  speedL = constrain(l, -MAX_SPEED, MAX_SPEED);
  speedR = constrain(r, -MAX_SPEED, MAX_SPEED);

  setSide(ENB_PIN, IN4_PIN, IN3_PIN, speedL);
  setSide(ENA_PIN, IN2_PIN, IN1_PIN, speedR);
}

void stopMotors() {
  setMotors(0, 0);
}

void initMotors() {
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  stopMotors();
}