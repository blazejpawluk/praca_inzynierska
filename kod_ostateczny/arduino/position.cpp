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
const uint8_t SLOTS_PER_REV = 20; // liczba szczelin tarczy enkodera
const uint8_t PULSES_PER_REV = 2 * SLOTS_PER_REV; // liczone są oba zbocza każdej szczeliny
const float DIST_PER_PULSE_MM = PI * WHEEL_DIAMETER_MM / PULSES_PER_REV; // przejechana odległość na impuls enkodera [mm]

// konfiguracja
const uint8_t ENC_WINDOW = 10; // do wyliczenia prędkości używamy tyle ostatnich próbek
// o nową próbkę MPU pytamy dopiero po 90% okresu od ostatniego odczytu
// (zapas 10%, aby opóźnienia pętli nie powodowały gubienia próbek)
const unsigned long MPU_POLL_US = MPU_SAMPLE_US * 9 / 10;
const unsigned long COAST_MS = 300; // czas wybiegu: po wyłączeniu silnika ruch jest liczony jeszcze przez ten czas

// stan
static unsigned long prevL, prevR; // liczniki enkoderów przy poprzedniej próbce
static unsigned long prevUs; // czas poprzedniej próbki [us]
static float prevGz = 0; // prędkość kątowa z poprzedniej próbki [stopnie/s]
static unsigned long lastRunMsL, lastRunMsR; // czas ostatniej pracy silników [ms]

// poza zachowywana przez reset: sekcja .noinit nie jest zerowana przy starcie programu, więc po resecie
// zawiera ostatni zapis, a po włączeniu zasilania przypadkowe dane (odrzucane przez sumę kontrolną);
// zapisy trafiają na zmianę do dwóch miejsc, aby reset w trakcie zapisu nie zniszczył ostatniej poprawnej pozy
struct SavedPose {
  float x, y, heading;
  uint16_t seq; // numer zapisu (wygrywa nowszy)
  uint16_t check; // suma kontrolna poprzednich pól
};
static SavedPose savedPoses[2] __attribute__((section(".noinit")));
static uint16_t saveSeq = 0;
const uint16_t SAVED_POSE_SALT = 0x5A3C; // odróżnia zapis od wyzerowanej pamięci
const float SAVED_POSE_MAX_MM = 1e6; // dalsze pozycje uznawane są za błędne

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
  lastRunMsL = lastRunMsR = millis() - COAST_MS;
}

static uint16_t poseChecksum(const SavedPose& p) {
  const uint8_t* bytes = (const uint8_t*)&p;
  uint16_t sum = SAVED_POSE_SALT;
  for (uint8_t i = 0; i < offsetof(SavedPose, check); i++) sum = ((sum << 1) | (sum >> 15)) ^ bytes[i];
  return sum;
}

static void savePose() {
  SavedPose& p = savedPoses[++saveSeq & 1];
  p = {posX, posY, heading, saveSeq, 0};
  p.check = poseChecksum(p);
}

static bool isSavedPoseValid(const SavedPose& p) {
  // porównania z NaN są fałszywe, więc odrzucają też uszkodzone liczby
  return p.check == poseChecksum(p) && fabs(p.x) < SAVED_POSE_MAX_MM && fabs(p.y) < SAVED_POSE_MAX_MM
      && p.heading > -180 && p.heading <= 180;
}

bool positionRestore() {
  const SavedPose* newest = NULL;
  for (uint8_t i = 0; i < 2; i++) {
    const SavedPose& p = savedPoses[i];
    if (isSavedPoseValid(p) && (!newest || (int16_t)(p.seq - newest->seq) > 0)) newest = &p;
  }
  if (!newest) return false;

  posX = newest->x;
  posY = newest->y;
  heading = newest->heading;
  saveSeq = newest->seq;
  return true;
}

void positionReset() {
  posX = 0;
  posY = 0;
  heading = 0;
  velocity = 0;
  savePose();
}

Pose positionGet() {
  return {posX, posY, heading};
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

// true, jeśli silnik pracuje lub od jego wyłączenia nie minął COAST_MS
static bool wheelActive(int speed, unsigned long& lastRunMs, unsigned long nowMs) {
  if (speed != 0) lastRunMs = nowMs;
  return nowMs - lastRunMs < COAST_MS;
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

  // impulsy i obrót przy postoju to zakłócenia lub drgania, a nie ruch pojazdu
  unsigned long nowMs = millis();
  bool activeL = wheelActive(motorSpeedL, lastRunMsL, nowMs);
  bool activeR = wheelActive(motorSpeedR, lastRunMsR, nowMs);
  if (!activeL) dl = 0;
  if (!activeR) dr = 0;

  velocity = updateVelocity(dl, dr, dtUs);

  float ds = (dirL * dl + dirR * dr) * DIST_PER_PULSE_MM / 2;

  // przy błędzie odczytu przyjmujemy brak obrotu w tej próbce
  float gz = 0;
  if (activeL || activeR) mpuReadGz(gz);

  float dTheta = (prevGz + gz) / 2 * dt;
  prevGz = gz;

  float rad = radians(heading + dTheta / 2);
  posX += ds * cos(rad);
  posY += ds * sin(rad);

  heading += dTheta;
  if (heading > 180) heading -= 360;
  else if (heading <= -180) heading += 360;

  savePose();
}

void positionPoll() {
  if (micros() - prevUs >= MPU_POLL_US && mpuDataReady()) {
    positionUpdate();
  }
}