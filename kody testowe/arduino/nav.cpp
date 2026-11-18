/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja poleceń trybu mapowania (opis w nav.h)
*/

#include "nav.h"
#include "servo.h"
#include "sonar.h"
#include "wheels.h"

// geometria (odległości mierzone od sonaru)
const float NAV_HALF_WIDTH_MM = 100; // połowa szerokości korytarza jazdy (pojazd + zapas)
const float NAV_STOP_MM = 240; // przeszkoda bliżej niż tyle przed sonarem zatrzymuje pojazd
const float NAV_BEAM_HALF_DEG = 7.5; // połowa kąta wiązki
const float NAV_BLIND_MM = 400; // dopuszczalna droga bez echa z przodu

// serwo i sonar
const int SERVO_FRONT = 90;
const uint8_t SWEEP[] = {90, 65, 90, 115, 90, 40, 90, 140}; // co drugi pomiar na wprost
const uint8_t SWEEP_LEN = sizeof(SWEEP);
const unsigned long SERVO_BASE_MS = 40; // uspokojenie serwa po ruchu
const unsigned long SERVO_MS_PER_DEG = 2; // czas obrotu serwa
const unsigned long PING_GAP_MS = 40; // odstęp między pomiarami (wygaszenie poprzedniego echa)
const unsigned long STOP_SETTLE_MS = 350; // wytracenie prędkości przed podaniem pozy lub pomiarem
const uint8_t NAV_SCAN_PINGS = 3;
const unsigned int SCAN_MAX_SPREAD_MM = 30; // największa różnica dwóch poprawnych pomiarów

// regulacja
const unsigned long CONTROL_MS = 20;
const unsigned long STALL_MS = 3000; // brak postępu przez ten czas = pojazd zablokowany
const float DRIVE_SPEED_MMS = 110;
const float DRIVE_SLOW_MMS = 60;
const float DRIVE_SLOW_DIST_MM = 60; // zwalnianie przed celem
const float DRIVE_PROGRESS_MM = 5;
const float DRIVE_KP = 6; // [PWM / stopień]
const int DRIVE_STEER_MAX = 50;
const int DRIVE_PWM_START = 90, DRIVE_PWM_MIN = 60, DRIVE_PWM_MAX = 200;
const float TURN_TOLERANCE_DEG = 1.5;
const float TURN_RATE_KP = 2.5; // [(stopień/s) / stopień]
const float TURN_RATE_MIN = 25, TURN_RATE_MAX = 70; // [stopnie/s]
const float TURN_PROGRESS_DEG = 2;
const int TURN_PWM_START = 120, TURN_PWM_MIN = 90, TURN_PWM_MAX = 230;
const uint8_t TURN_CORRECTIONS = 3;

enum NavState : uint8_t {NAV_IDLE, NAV_SCAN, NAV_TURN, NAV_DRIVE, NAV_SETTLE};

static NavState state = NAV_IDLE;
static int currentId = 0;
static NavResult finalResult = NAV_RUNNING;
static bool reportPending = false;

static unsigned long waitUntilMs = 0; // najbliższy pomiar sonaru
static unsigned long settleUntilMs = 0;
static unsigned long motorsOffMs = 0;
static unsigned long lastControlMs = 0;
static bool sonarWaiting = false; // czekamy na własny pomiar

// pomiar z postoju
static unsigned int samples[NAV_SCAN_PINGS];
static uint8_t pings = 0, validPings = 0;
static unsigned int scanMm = 0;
static Pose scanPose = {0, 0, 0};

// obrót i jazda
static float targetHeading = 0;
static int pwm = 0;
static unsigned long progressMs = 0;
static float progressRef = 0; // punkt odniesienia wykrywania braku ruchu

// obrót
static bool turnSettling = false;
static uint8_t corrections = 0;

// jazda
static float driveDist = 0, driveCos = 0, driveSin = 0, startX = 0, startY = 0, travelled = 0;
static int8_t driveDir = 1;
static bool frontChecked = false;
static uint8_t sweepIndex = 0;
static float lastEchoAt = 0; // droga przy ostatnim echu z przodu

static float wrap180(float deg) {
  while (deg > 180) deg -= 360;
  while (deg <= -180) deg += 360;
  return deg;
}

static bool reached(unsigned long t, unsigned long now) {
  return (long)(now - t) >= 0;
}

static void moveServo(int angle, unsigned long now) {
  unsigned long travelMs = SERVO_BASE_MS + abs(angle - servoAngle) * SERVO_MS_PER_DEG;
  servoSetAngle(angle);
  waitUntilMs = now + max(travelMs, PING_GAP_MS);
}

static void stopMotors(unsigned long now) {
  if (motorSpeedL != 0 || motorSpeedR != 0) {
    motorsSet(0, 0);
    motorsOffMs = now;
  }
}

static void finish(NavResult result, unsigned long now) {
  bool moving = motorSpeedL != 0 || motorSpeedR != 0;
  stopMotors(now);
  sonarWaiting = false;
  finalResult = result;
  state = NAV_SETTLE;
  settleUntilMs = now + (moving ? STOP_SETTLE_MS : 0);
}

// początek polecenia; false oznacza powtórzenie bieżącego
static bool begin(int id, NavState newState, unsigned long now) {
  if (id == currentId) {
    if (state == NAV_IDLE) reportPending = true;
    return false;
  }
  stopMotors(now);
  currentId = id;
  state = newState;
  finalResult = NAV_RUNNING;
  reportPending = false;
  sonarWaiting = false;
  scanMm = 0;
  progressMs = now;
  lastControlMs = now;
  return true;
}

bool navScan(int id, int angle) {
  unsigned long now = millis();
  if (!begin(id, NAV_SCAN, now)) return false;

  pings = 0;
  validPings = 0;
  scanMm = 0;
  moveServo(angle, now);
  unsigned long settled = motorsOffMs + STOP_SETTLE_MS;
  if ((long)(settled - waitUntilMs) > 0 && (long)(settled - now) > 0) waitUntilMs = settled;
  return true;
}

bool navTurn(int id, int headingCdeg) {
  unsigned long now = millis();
  if (!begin(id, NAV_TURN, now)) return false;

  targetHeading = headingCdeg / 100.0f;
  pwm = TURN_PWM_START;
  turnSettling = false;
  corrections = 0;
  progressRef = heading;
  return true;
}

bool navDrive(int id, int mm, int headingCdeg) {
  unsigned long now = millis();
  if (!begin(id, NAV_DRIVE, now)) return false;

  targetHeading = headingCdeg / 100.0f;
  driveDir = mm >= 0 ? 1 : -1;
  driveDist = abs(mm);
  float rad = radians(targetHeading);
  driveCos = cos(rad);
  driveSin = sin(rad);
  startX = posX;
  startY = posY;
  travelled = 0;
  progressRef = 0;
  lastEchoAt = 0;
  pwm = DRIVE_PWM_START;
  sweepIndex = 0;

  // jazda do tyłu odbywa się tylko po własnym śladzie, bez sprawdzania sonarem
  frontChecked = driveDir < 0;
  moveServo(SERVO_FRONT, now);
  return true;
}

void navAbort() {
  if (state == NAV_IDLE) return;
  stopMotors(millis());
  sonarWaiting = false;
  finalResult = NAV_ABORTED;
  state = NAV_IDLE;
  reportPending = true;
}

bool navActive() {
  return state != NAV_IDLE;
}

// zmienia wypełnienie PWM tak, aby prędkość dążyła do zadanej
static void adjustPwm(float actual, float wanted, int low, int high) {
  pwm = constrain(pwm + (actual < wanted ? 1 : -1), low, high);
}

static void controlTurn(unsigned long now) {
  float error = wrap180(targetHeading - heading);

  if (turnSettling) {
    if (!reached(settleUntilMs, now)) return;
    if (fabs(error) <= TURN_TOLERANCE_DEG || corrections >= TURN_CORRECTIONS) {
      finish(NAV_DONE, now);
      return;
    }
    corrections++;
    turnSettling = false;
    pwm = TURN_PWM_START;
    progressRef = heading;
    progressMs = now;
  }

  if (fabs(error) <= TURN_TOLERANCE_DEG) {
    stopMotors(now);
    turnSettling = true;
    settleUntilMs = now + STOP_SETTLE_MS;
    return;
  }

  if (fabs(wrap180(heading - progressRef)) >= TURN_PROGRESS_DEG) {
    progressRef = heading;
    progressMs = now;
  } else if (now - progressMs > STALL_MS) {
    finish(NAV_STALLED, now);
    return;
  }

  float wantedRate = constrain(fabs(error) * TURN_RATE_KP, TURN_RATE_MIN, TURN_RATE_MAX);
  adjustPwm(fabs(yawRate), wantedRate, TURN_PWM_MIN, TURN_PWM_MAX);
  int dir = error > 0 ? 1 : -1; // dodatni błąd = obrót w lewo
  motorsSet(-dir * pwm, dir * pwm);
}

static void controlDrive(unsigned long now) {
  travelled = driveDir * ((posX - startX) * driveCos + (posY - startY) * driveSin);
  float remaining = driveDist - travelled;
  if (remaining <= 0) {
    finish(NAV_DONE, now);
    return;
  }

  if (travelled - progressRef >= DRIVE_PROGRESS_MM) {
    progressRef = travelled;
    progressMs = now;
  } else if (now - progressMs > STALL_MS) {
    finish(NAV_STALLED, now);
    return;
  }

  float wantedSpeed = remaining < DRIVE_SLOW_DIST_MM ? DRIVE_SLOW_MMS : DRIVE_SPEED_MMS;
  adjustPwm(fabs(velocity), wantedSpeed, DRIVE_PWM_MIN, DRIVE_PWM_MAX);
  int steer = constrain(wrap180(targetHeading - heading) * DRIVE_KP, -DRIVE_STEER_MAX, DRIVE_STEER_MAX);
  motorsSet(driveDir * pwm - steer, driveDir * pwm + steer);
}

// true, jeśli jakikolwiek punkt łuku wiązki leży w korytarzu jazdy przed pojazdem
static bool inCorridor(unsigned int mm, int angle) {
  for (int8_t side = -1; side <= 1; side++) {
    float rad = radians(angle - SERVO_FRONT + side * NAV_BEAM_HALF_DEG);
    float forward = mm * cos(rad);
    float lateral = mm * sin(rad);
    if (forward > 0 && forward < NAV_STOP_MM && fabs(lateral) < NAV_HALF_WIDTH_MM) return true;
  }
  return false;
}

static void onDriveMeasurement(unsigned int mm, int angle, unsigned long now) {
  if (mm != 0 && inCorridor(mm, angle)) {
    finish(NAV_OBSTACLE, now);
    return;
  }

  // brak echa nie oznacza wolnej drogi, więc jazda bez echa z przodu ma ograniczony zasięg
  if (angle == SERVO_FRONT) {
    if (mm != 0) lastEchoAt = travelled;
    else if (travelled - lastEchoAt > NAV_BLIND_MM) {
      finish(NAV_BLIND, now);
      return;
    }
    frontChecked = true;
  }

  sweepIndex = (sweepIndex + 1) % SWEEP_LEN;
  moveServo(SWEEP[sweepIndex], now);
}

// wynik z NAV_SCAN_PINGS pomiarów: mediana z trzech lub średnia z dwóch zgodnych, inaczej brak echa
static unsigned int scanResult() {
  for (uint8_t i = 1; i < validPings; i++) {
    unsigned int v = samples[i];
    uint8_t j = i;
    for (; j > 0 && samples[j - 1] > v; j--) samples[j] = samples[j - 1];
    samples[j] = v;
  }
  if (validPings == NAV_SCAN_PINGS) return samples[1];
  if (validPings == 2 && samples[1] - samples[0] <= SCAN_MAX_SPREAD_MM) return (samples[0] + samples[1]) / 2;
  return 0;
}

bool navOnMeasurement(unsigned int mm, int angle, const Pose& pose) {
  if (!sonarWaiting) return false;
  sonarWaiting = false;
  unsigned long now = millis();

  if (state == NAV_DRIVE) {
    onDriveMeasurement(mm, angle, now);
    return false; // pomiar z jazdy jest wysyłany jako A:DIST
  }
  if (state != NAV_SCAN) return false;

  if (pings == 0) scanPose = pose;
  pings++;
  if (mm != 0) samples[validPings++] = mm;
  waitUntilMs = now + PING_GAP_MS;
  if (pings == NAV_SCAN_PINGS) {
    scanMm = scanResult();
    finish(NAV_DONE, now);
  }
  return true;
}

void navUpdate() {
  if (state == NAV_IDLE) return;
  unsigned long now = millis();

  if (state == NAV_SETTLE) {
    if (!reached(settleUntilMs, now)) return;
    state = NAV_IDLE;
    reportPending = true;
    return;
  }

  // pomiary sonaru (przy jeździe do tyłu nie są wykonywane)
  bool usesSonar = state == NAV_SCAN || (state == NAV_DRIVE && driveDir > 0);
  if (usesSonar && !sonarWaiting && reached(waitUntilMs, now) && !sonarIsBusy()) {
    sonarStart();
    sonarWaiting = true;
  }

  if (now - lastControlMs < CONTROL_MS) return;
  lastControlMs = now;
  if (state == NAV_TURN) controlTurn(now);
  else if (state == NAV_DRIVE && frontChecked) controlDrive(now);
}

bool navTakeReport(NavReport& report) {
  if (!reportPending) return false;
  reportPending = false;

  report.id = currentId;
  report.result = finalResult;
  report.servo = servoAngle;
  report.mm = scanMm;
  report.pose = scanMm != 0 ? scanPose : positionGet();
  return true;
}
