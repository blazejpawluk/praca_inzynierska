/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka śledząca aktualną pozycję pojazdu

  Korzysta z danych z enkoderów szczelinowych kół do wyznaczania przejechanej odległości
  oraz żyroskopu do wyznaczania kąta, o który obrócił się pojazd
*/

#pragma once

#include <Arduino.h>
#include "mpu.h"
#include "encoders.h"

// dane
float posX, posY;
float angle;
float velocity;

// parametry techniczne
const uint8_t WHEEL_DIAMETER = 65; // średnica kół [mm]
const uint8_t PULSES_PER_REV = 20; // liczba szelin tarczy enkodera
const float DIST_PER_PULSE = PI * WHEEL_DIAMETER / PULSES_PER_REV; // przejechana odległość na puls enkodera [mm]

// stałe
const uint8_t ENC_WINDOW = 10; // do wyliczenia prędkości używamy tyle ostatnich pomiarów z enkoderów

// zmienne systemu
extern int speedL, speedR;
unsigned long prevL, prevR;
float prevGz = 0;
int8_t dirL, dirR;
int16_t ringPulses[ENC_WINDOW];
uint32_t ringDtUs[ENC_WINDOW];
uint8_t ringIdx = 0;
unsigned long prevUs;

void initPosition() {
  readEncoders(prevL, prevR);
  memset(ringPulses, 0, sizeof(ringPulses));
  memset(ringDtUs, 0, sizeof(ringDtUs));
  prevGz = 0;
  prevUs = micros();
}

void resetPosition() {
  posX = 0;
  posY = 0;
  angle = 0;
  velocity = 0;
}

float encodeVelocity(long dl, long dr, float dt) {
  if (speedL != 0) dirL = speedL > 0 ? 1 : -1;
  if (speedR != 0) dirR = speedR > 0 ? 1 : -1;

  ringPulses[ringIdx] = dirL * dl + dirR * dr;
  ringDtUs[ringIdx] = dt * 1e6;
  ringIdx = (ringIdx + 1) % ENC_WINDOW;

  long pulses = 0; unsigned long us = 0;
  for (uint8_t i = 0; i < ENC_WINDOW; i++) {
    pulses += ringPulses[i];
    us += ringDtUs[i];
  }
  if (us == 0) return 0.0;
  return pulses * DIST_PER_PULSE / 2 / (us / 1e6);
}

void updatePosition() {
  unsigned long nowUs = micros();
  float dt = (nowUs - prevUs) / 1e6;
  prevUs = nowUs;

  unsigned long l, r;
  readEncoders(l, r);
  long dl = (long)(l - prevL);
  long dr = (long)(r - prevR);
  prevL = l; prevR = r;
  velocity = encodeVelocity(dl, dr, dt);

  float w = 0; readGz(w);
  angle += (prevGz + w) / 2 * dt;
  prevGz = w;

  if (angle > 180) angle -= 360;
  else if (angle <= -180) angle += 360;
  float rad = radians(angle);

  posX += velocity * cos(rad) * dt;
  posY += velocity * sin(rad) * dt;
}