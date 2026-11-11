// Błażej Pawluk
// wyliczanie pozycji

#pragma once

#include <Arduino.h>
#include "mpu.h"
#include "encoders.h"
#include "wheels.h"

// zbierane dane
float posX = 0, posY = 0;
float heading = 0;
float vel = 0;

// parametry techniczne
const float WHEEL_DIAMETER_M = 0.065; // średnica kół [m]
const uint8_t PULSES_PER_REV = 20; // liczba szczelin tarczy enkodera
const float M_PER_PULSE = PI * WHEEL_DIAMETER_M / PULSES_PER_REV; // przejechana odległość na 1 puls

// stałe systemu
const float G = 9.81; // m/s^2 na 1g
const uint8_t ENC_WINDOW = 10;

// zmienne systemu
unsigned long prevL = 0, prevR = 0;
float prevGz = 0;
int8_t dirL = 1, dirR = 1;
int8_t ringPulses[ENC_WINDOW];
uint16_t ringDtUs[ENC_WINDOW];
uint8_t ringIdx = 0;
unsigned long prevUs = 0;

void initPosition() {
  readEncoders(prevL, prevR);
  prevUs = micros();
}

void resetPosition() {
  posX = posY = 0;
  heading = 0;
  vel = 0;
}

float encodeVelocity(long dl, long dr, float dt) {
  if (speedL != 0) dirL = speedL > 0 ? 1 : -1;
  if (speedR != 0) dirR = speedR > 0 ? 1 : -1;

  ringPulses[ringIdx] = constrain(dirL * dl + dirR * dr, -127, 127);
  ringDtUs[ringIdx] = min(dt * 1e6, 65535.0);
  ringIdx = (ringIdx + 1) % ENC_WINDOW;

  long pulses = 0;
  unsigned long us = 0;
  for (uint8_t i = 0; i < ENC_WINDOW; i++) {
    pulses += ringPulses[i];
    us += ringDtUs[i];
  }
  if (us == 0) return 0.0;
  return pulses * M_PER_PULSE / 2 / (us / 1e6);
}

void updatePosition() {
  unsigned long nowUs = micros();
  float dt = (nowUs - prevUs) / 1e6;
  prevUs = nowUs;

  // enkodery
  unsigned long l, r;
  readEncoders(l, r);
  long dl = (long)(l - prevL);
  long dr = (long)(r - prevR);
  prevL = l;
  prevR = r;
  vel = encodeVelocity(dl, dr, dt);

  // kąt
  float w = gz;
  heading += (prevGz + w) / 2 * dt;
  prevGz = w;
  if (heading > 180) heading -= 360;
  else if (heading <= -180) heading += 360;

  // pozycja
  float rad = radians(heading);
  posX += vel * cos(rad) * dt;
  posY += vel * sin(rad) * dt;
}