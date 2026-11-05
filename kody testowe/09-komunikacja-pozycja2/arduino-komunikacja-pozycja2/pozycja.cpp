// Błażej Pawluk
// wyliczanie pozycji i kąta autka: droga z enkoderów, kąt z żyroskopu, filtr Kalmana prędkości (enkodery + akcelerometr)
// układ współrzędnych ustalany przy starcie: X - przód, Y - lewo, kąt - skręt w lewo

// sposób liczenia (co próbkę MPU, 100 Hz, czas mierzony przez micros()):
//    kąt: całka z gz (metoda trapezów) po odjęciu dryfu
//    prędkość: filtr Kalmana - predykcja z akcelerometru (ax), korekcja prędkością z enkoderów
//              duża niezgodność enkoderów z predykcją = poślizg kół, wtedy enkodery są pomijane
//    pozycja: całka z prędkości w kierunku aktualnego kąta
//    postój (wyłączone silniki i brak impulsów przez STILL_MS): prędkość = 0, kąt zamrożony,
//              dopasowanie dryfu żyroskopu (gz) i akcelerometru (ax)

#include "pozycja.h"
#include "zyroskop.h"
#include "enkodery.h"
#include "silniki.h"

// parametry mechaniczne (do zmierzenia na autku)
const float WHEEL_DIAMETER_M = 0.065;
const uint8_t PULSES_PER_REV = 20; // liczba szczelin tarczy enkodera (liczone są tylko zbocza narastające)
const float M_PER_PULSE = PI * WHEEL_DIAMETER_M / PULSES_PER_REV;

const float G = 9.81; // m/s^2 na 1 g
const float MAX_DT_S = 0.1; // dłuższa przerwa między próbkami jest przycinana

// prędkość z enkoderów liczona w oknie ostatnich próbek (pojedynczy impuls to ~1 cm)
const uint8_t ENC_WINDOW = 10; // 10 próbek po 10 ms = 100 ms

// postój
const unsigned long STILL_MS = 500;
const float BIAS_RATE = 0.01; // szybkość dopasowania dryfu podczas postoju (na próbkę, ~1 s)
const float BIAS_MAX_GZ = 3.0;  // stopnie/s, większe odczyty to ruch (np. obracanie ręką), a nie dryf
const float BIAS_MAX_AX = 0.05; // g

// filtr Kalmana prędkości
const float ACC_NOISE = 0.5;  // m/s^2, niepewność przyspieszenia z akcelerometru (szum procesu Q)
const float ENC_NOISE = 0.05; // m/s, niepewność prędkości z enkoderów (szum pomiaru R)
const float SLIP_GATE = 3.0;  // próg poślizgu w odchyleniach standardowych
const unsigned long SLIP_MAX_MS = 1000; // dłuższy "poślizg" - przyjęcie prędkości z enkoderów

float posX = 0, posY = 0;
float heading = 0;
float vel = 0;
bool slip = false;
bool still = true;

float velVar = 0; // wariancja prędkości (P w filtrze Kalmana)
float gzBias = 0, axBias = 0; // dryf dopasowywany podczas postoju
float prevGz = 0;
unsigned long prevL = 0, prevR = 0;
int8_t dirL = 1, dirR = 1; // kierunek obrotu kół (enkodery nie rozróżniają kierunku)
int8_t ringPulses[ENC_WINDOW]; // suma impulsów obu kół ze znakiem w każdej próbce
uint16_t ringDtUs[ENC_WINDOW];
uint8_t ringIdx = 0;
unsigned long prevUs = 0;
unsigned long lastActiveMs = 0; // ostatni moment ruchu (impulsy lub włączone silniki)
unsigned long slipStartMs = 0;

void initPosition() {
  readEncoders(prevL, prevR);
  prevUs = micros();
  lastActiveMs = millis();
}

void resetPosition() {
  posX = posY = 0;
  heading = 0;
  vel = 0;
  velVar = 0;
}

// prędkość z enkoderów w m/s (średnia obu kół z ostatnich ENC_WINDOW próbek)
float encoderVelocity(long dL, long dR, float dt) {
  // kierunek z polecenia silnika, przy zatrzymanych silnikach (wybieg) - ostatni znany
  if (speedL != 0) dirL = speedL > 0 ? 1 : -1;
  if (speedR != 0) dirR = speedR > 0 ? 1 : -1;

  ringPulses[ringIdx] = constrain(dirL * dL + dirR * dR, -127, 127);
  ringDtUs[ringIdx] = min(dt * 1e6, 65535.0);
  ringIdx = (ringIdx + 1) % ENC_WINDOW;

  long pulses = 0;
  unsigned long us = 0;
  for (uint8_t i = 0; i < ENC_WINDOW; i++) {
    pulses += ringPulses[i];
    us += ringDtUs[i];
  }
  if (us == 0) return 0;
  return pulses * M_PER_PULSE / 2 / (us / 1e6);
}

void updatePosition() {
  unsigned long nowUs = micros();
  unsigned long nowMs = millis();
  float dt = (nowUs - prevUs) / 1e6;
  prevUs = nowUs;
  if (dt > MAX_DT_S) dt = MAX_DT_S;

  // enkodery
  unsigned long l, r;
  readEncoders(l, r);
  long dL = (long)(l - prevL);
  long dR = (long)(r - prevR);
  prevL = l;
  prevR = r;
  float vEnc = encoderVelocity(dL, dR, dt);

  // postój: zerowanie prędkości i dopasowanie dryfu
  if (dL != 0 || dR != 0 || speedL != 0 || speedR != 0) lastActiveMs = nowMs;
  still = nowMs - lastActiveMs >= STILL_MS;
  if (still) {
    if (fabs(gz - gzBias) < BIAS_MAX_GZ) gzBias += BIAS_RATE * (gz - gzBias);
    if (fabs(ax - axBias) < BIAS_MAX_AX) axBias += BIAS_RATE * (ax - axBias);
    vel = 0;
    velVar = 0;
    slip = false;
    prevGz = 0;
    return;
  }

  // kąt
  float w = gz - gzBias;
  heading += (prevGz + w) / 2 * dt;
  prevGz = w;
  if (heading > 180) heading -= 360;
  else if (heading <= -180) heading += 360;

  // filtr Kalmana: predykcja z akcelerometru
  vel += (ax - axBias) * G * dt;
  velVar += sq(ACC_NOISE * dt);

  // filtr Kalmana: korekcja prędkością z enkoderów (z wykrywaniem poślizgu)
  float r2 = sq(ENC_NOISE);
  float innov = vEnc - vel;
  float s = velVar + r2;
  if (sq(innov) > sq(SLIP_GATE) * s) {
    if (!slip) {
      slip = true;
      slipStartMs = nowMs;
    } else if (nowMs - slipStartMs > SLIP_MAX_MS) {
      vel = vEnc;
      velVar = r2;
      slip = false;
    }
  } else {
    slip = false;
    float k = velVar / s;
    vel += k * innov;
    velVar *= 1 - k;
  }

  // pozycja
  float rad = radians(heading);
  posX += vel * cos(rad) * dt;
  posY += vel * sin(rad) * dt;
}
