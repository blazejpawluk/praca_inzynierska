/*
 * TEST SERWA - CZĘŚĆ 2: RUCH W ZAKRESIE ±45° OD ŚRODKA
 * Serwo              -> D9
 * Lewy enkoder (DO)  -> A0  (ok. 1/4 obrotu lewym kołem  = poprzedni tryb)
 * Prawy enkoder (DO) -> A1  (ok. 1/4 obrotu prawym kołem = następny tryb)
 * LCD I2C 16x2       -> SDA = A4, SCL = A5
 *
 * Środek: 1500 us (skalibrowany). Zakres: 45° w lewo i 45° w prawo.
 *
 * Tryby:
 *   1/4 Pozycje    - środek -> L45 -> środek -> P45 (po 2 s)
 *   2/4 Kroki 15st - L45..P45 co 15° i z powrotem (po 1 s)
 *   3/4 Plynnie    - ciągłe przemiatanie L45 <-> P45
 *   4/4 Skoki L-P  - gwałtowne skoki L45 <-> P45 (test zasilania)
 *
 * Ekran:
 *   1/4 Pozycje
 *   Kat L45  1036us      <- kierunek i kąt od środka + wysyłany puls
 *
 * Wymagana biblioteka: "LiquidCrystal I2C" (Frank de Brabander)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ---------------- KONFIGURACJA ----------------
const uint8_t LCD_ADDR = 0x27;          // jeśli ekran pusty, spróbuj 0x3F
const uint8_t SERVO_PIN = 9;

const int CENTER_US = 1500;             // skalibrowany środek
const int MAX_ANGLE = 45;               // maks. wychylenie w każdą stronę
const float US_PER_DEG = 1856.0 / 180.0; // ~10.3 us/° (jak w Servo.h: 544..2400 us = 180°)
const bool INVERT_DIR = false;          // true, jeśli przy "L" serwo kręci w prawo

const unsigned long POS_HOLD_MS = 2000; // tryb 1
const unsigned long STEP_HOLD_MS = 1000; // tryb 2
const unsigned long SWEEP_STEP_MS = 15; // tryb 3: 1° co 15 ms (~67°/s)
const unsigned long FAST_HOLD_MS = 1000; // tryb 4

const uint8_t PULSES_TO_SWITCH = 5;     // ile szczelin enkodera = zmiana trybu
const unsigned long DEBOUNCE_US = 2000;
const unsigned long LCD_REFRESH_MS = 150;
// ----------------------------------------------

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
Servo servo;

// --- Sekwencje kątów (ujemny = lewo, dodatni = prawo) ---
const int SEQ_POS[]  = {0, -MAX_ANGLE, 0, MAX_ANGLE};
const int SEQ_STEP[] = {-45, -30, -15, 0, 15, 30, 45, 30, 15, 0, -15, -30};
const int SEQ_FAST[] = {-MAX_ANGLE, MAX_ANGLE};

const uint8_t MODE_COUNT = 4;
const char* const MODE_NAMES[MODE_COUNT] = {"Pozycje", "Kroki 15st", "Plynnie", "Skoki L-P"};

// --- Enkodery jako przełącznik trybów ---
volatile uint8_t pulsesL = 0;
volatile uint8_t pulsesR = 0;
volatile unsigned long lastL = 0;
volatile unsigned long lastR = 0;
volatile uint8_t prevState = 0;

ISR(PCINT1_vect) {
  uint8_t state = PINC & 0b00000011;    // bit0 = A0 (lewy), bit1 = A1 (prawy)
  uint8_t rising = state & ~prevState;
  prevState = state;
  unsigned long now = micros();

  if ((rising & 0b01) && (now - lastL > DEBOUNCE_US)) {
    if (pulsesL < 255) pulsesL++;
    lastL = now;
  }
  if ((rising & 0b10) && (now - lastR > DEBOUNCE_US)) {
    if (pulsesR < 255) pulsesR++;
    lastR = now;
  }
}

// --- Stan testu ---
uint8_t mode = 0;
uint8_t seqIndex = 0;
int angle = 0;                          // aktualny kąt od środka
int sweepDir = 1;
unsigned long lastMoveMs = 0;
unsigned long lastLcdMs = 0;
bool lcdDirty = true;

int angleToUs(int a) {
  if (INVERT_DIR) a = -a;
  // większy puls = obrót w lewo (typowo dla serw patrząc od góry)
  return CENTER_US - (int)lround(a * US_PER_DEG);
}

void moveTo(int a) {
  angle = constrain(a, -MAX_ANGLE, MAX_ANGLE);
  servo.writeMicroseconds(angleToUs(angle));
  lcdDirty = true;
}

void showMode() {
  char line[17];
  snprintf(line, sizeof(line), "%u/%u %-11s", mode + 1, MODE_COUNT, MODE_NAMES[mode]);
  lcd.setCursor(0, 0);
  lcd.print(line);
}

void showAngle() {
  char line[17];
  char side = (angle < 0) ? 'L' : (angle > 0) ? 'P' : ' ';
  if (angle == 0) {
    snprintf(line, sizeof(line), "Kat srod %4dus ", angleToUs(angle));
  } else {
    snprintf(line, sizeof(line), "Kat %c%2d  %4dus ", side, abs(angle), angleToUs(angle));
  }
  lcd.setCursor(0, 1);
  lcd.print(line);
}

void startMode(uint8_t m) {
  mode = m;
  seqIndex = 0;
  sweepDir = 1;
  showMode();
  switch (mode) {
    case 0: moveTo(SEQ_POS[0]);  break;
    case 1: moveTo(SEQ_STEP[0]); break;
    case 2: moveTo(-MAX_ANGLE);  break;
    case 3: moveTo(SEQ_FAST[0]); break;
  }
  lastMoveMs = millis();
}

void runSequence(const int* seq, uint8_t len, unsigned long holdMs, unsigned long now) {
  if (now - lastMoveMs < holdMs) return;
  lastMoveMs = now;
  seqIndex = (seqIndex + 1) % len;
  moveTo(seq[seqIndex]);
}

void runSweep(unsigned long now) {
  if (now - lastMoveMs < SWEEP_STEP_MS) return;
  lastMoveMs = now;
  if (angle >= MAX_ANGLE)  sweepDir = -1;
  if (angle <= -MAX_ANGLE) sweepDir = 1;
  moveTo(angle + sweepDir);
}

void checkModeSwitch() {
  noInterrupts();
  uint8_t l = pulsesL;
  uint8_t r = pulsesR;
  interrupts();

  int8_t change = 0;
  if (r >= PULSES_TO_SWITCH) change = 1;
  else if (l >= PULSES_TO_SWITCH) change = -1;

  if (change != 0) {
    noInterrupts();
    pulsesL = 0;
    pulsesR = 0;
    interrupts();
    startMode((mode + MODE_COUNT + change) % MODE_COUNT);
  }
}

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Test serwa +-45");
  lcd.setCursor(0, 1);
  lcd.print("Srodek 1500us");

  servo.attach(SERVO_PIN);
  servo.writeMicroseconds(CENTER_US);
  delay(1500);
  lcd.clear();

  noInterrupts();
  prevState = PINC & 0b00000011;
  PCICR  |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9);
  interrupts();

  startMode(0);
}

void loop() {
  unsigned long now = millis();

  checkModeSwitch();

  switch (mode) {
    case 0: runSequence(SEQ_POS,  sizeof(SEQ_POS)  / sizeof(SEQ_POS[0]),  POS_HOLD_MS,  now); break;
    case 1: runSequence(SEQ_STEP, sizeof(SEQ_STEP) / sizeof(SEQ_STEP[0]), STEP_HOLD_MS, now); break;
    case 2: runSweep(now); break;
    case 3: runSequence(SEQ_FAST, sizeof(SEQ_FAST) / sizeof(SEQ_FAST[0]), FAST_HOLD_MS, now); break;
  }

  // LCD jest wolne (I2C), więc odświeżamy je rzadziej niż ruch serwa
  if (lcdDirty && now - lastLcdMs >= LCD_REFRESH_MS) {
    lastLcdMs = now;
    lcdDirty = false;
    showAngle();
  }
}