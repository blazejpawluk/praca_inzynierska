/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca sterowanie silnikami kół przez sterownik L298N

  Połączenie z L298N:
    - silnik prawy: ENA=5, IN1=4, IN2=3
    - silnik lewy: ENB=6, IN3=7, IN4=8
*/

#pragma once

#include <Arduino.h>

// piny
const uint8_t ENA_PIN = 5;
const uint8_t IN1_PIN = 4;
const uint8_t IN2_PIN = 3;
const uint8_t ENB_PIN = 6;
const uint8_t IN3_PIN = 7;
const uint8_t IN4_PIN = 8; 

// aktualne prędkości
int speedL, speedR;

void setSide(uint8_t en, uint8_t inA, uint8_t inB, int speed) {
  digitalWrite(inA, speed > 0 ? HIGH : LOW);
  digitalWrite(inB, speed < 0 ? HIGH : LOW);
  analogWrite(en, abs(speed));
}

// ujemna prędkość = jazda do tyłu
void setMotors(int l, int r) {
  speedL = l;
  speedR = r;

  setSide(ENB_PIN, IN4_PIN, IN3_PIN, speedL);
  setSide(ENA_PIN, IN2_PIN, IN1_PIN, speedR);
}

void initMotors() {
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);

  setMotors(0, 0);
}