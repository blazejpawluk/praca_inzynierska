/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja śledzenia pozycji pojazdu (opis w position.h)
*/

#include <Arduino.h>
#include "position.h"
#include "mpu.h"
#include "encoders.h"
#include "wheels.h"

float posX = 0, posY = 0;
float heading = 0;
float velocity = 0;

// parametry techniczne
const uint8_t WHEEL_DIAMETER_MM = 65; // średnica kół [mm]
const uint8_t PULSES_PER_REV = 20; // liczba szczelin tarczy enkodera
const float DIST_PER_PULSE_MM = PI * WHEEL_DIAMETER_MM / PULSES_PER_REV; // przejechana odległość na impuls enkodera [mm]

// konfiguracja
const uint8_t ENC_WINDOW = 10; // do wyliczenia prędkości używamy tyle ostatnich próbek
// o nową próbkę MPU pytamy dopiero po 90% okresu od ostatniego odczytu
// (zapas 10%, aby opóźnienia pętli nie powodowały gubienia próbek)
const unsigned long MPU_POLL_US = MPU_SAMPLE_US * 9 / 10;

// stan
static unsigned long prevL, prevR; // liczniki enkoderów przy poprzedniej próbce
static unsigned long prevUs; // czas poprzedniej próbki [us]
static float prevGz = 0; // prędkość kątowa z poprzedniej próbki [stopnie/s]

// kierunek obrotu kół: enkodery jednokanałowe nie rozpoznają kierunku, więc jest on brany
// ze znaku ostatniej niezerowej prędkości silnika
static int8_t dirL = 0, dirR = 0;

// bufor ostatnich ENC_WINDOW próbek do wyliczenia prędkości
static int8_t ringPulses[ENC_WINDOW]; // impulsy na próbkę (przycięte do +-127; przy próbkach co 5 ms zwykle < 10)
static uint16_t ringDtUs[ENC_WINDOW]; // czas próbki [us] (przycięty do 65535; zwykle ~5000)
static uint8_t ringIdx = 0;
static long sumPulses = 0; // suma ringPulses (aktualizowana przyrostowo)
static unsigned long sumUs = 0; // suma ringDtUs (aktualizowana przyrostowo)

void positionInit() {
  encodersRead(prevL, prevR);
  memset(ringPulses, 0, sizeof(ringPulses));
  memset(ringDtUs, 0, sizeof(ringDtUs));
  sumPulses = 0;
  sumUs = 0;
  prevGz = 0;
  prevUs = micros();
}

void positionReset() {
  posX = 0;
  posY = 0;
  heading = 0;
  velocity = 0;
}

// dodaje próbkę z enkoderów do bufora i zwraca średnią prędkość z ostatnich ENC_WINDOW próbek [mm/s];
// aktualizuje też kierunek obrotu kół (dirL, dirR)
static float updateVelocity(long dl, long dr, unsigned long dtUs) {
  if (motorSpeedL != 0) dirL = motorSpeedL > 0 ? 1 : -1;
  if (motorSpeedR != 0) dirR = motorSpeedR > 0 ? 1 : -1;

  // przycięcie do zakresu typów bufora (przekroczenie możliwe tylko po długiej przerwie w odczycie MPU)
  int8_t pulses = constrain(dirL * dl + dirR * dr, -127L, 127L);
  uint16_t dt16 = dtUs > 65535UL ? 65535 : dtUs;

  // zamiana najstarszej próbki na nową: sumy aktualizowane bez przechodzenia całego bufora
  sumPulses += pulses - ringPulses[ringIdx];
  sumUs += (unsigned long)dt16 - ringDtUs[ringIdx];
  ringPulses[ringIdx] = pulses;
  ringDtUs[ringIdx] = dt16;
  ringIdx = (ringIdx + 1) % ENC_WINDOW;

  if (sumUs == 0) return 0.0f;
  // średnia z dwóch kół: suma impulsów / 2
  return sumPulses * DIST_PER_PULSE_MM / 2 / (sumUs / 1e6f);
}

static void positionUpdate() {
  unsigned long nowUs = micros();
  unsigned long dtUs = nowUs - prevUs;
  float dt = dtUs / 1e6f;
  prevUs = nowUs;

  unsigned long l, r;
  encodersRead(l, r);
  long dl = (long)(l - prevL);
  long dr = (long)(r - prevR);
  prevL = l;
  prevR = r;
  velocity = updateVelocity(dl, dr, dtUs);

  // przy błędzie odczytu przyjmujemy brak obrotu w tej próbce
  float gz = 0;
  mpuReadGz(gz);

  // całkowanie metodą trapezów
  heading += (prevGz + gz) / 2 * dt;
  prevGz = gz;

  if (heading > 180) heading -= 360;
  else if (heading <= -180) heading += 360;
  float rad = radians(heading);

  posX += velocity * cos(rad) * dt;
  posY += velocity * sin(rad) * dt;
}

void positionPoll() {
  if (micros() - prevUs >= MPU_POLL_US && mpuDataReady()) {
    positionUpdate();
  }
}