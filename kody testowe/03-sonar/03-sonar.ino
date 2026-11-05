// Błażej Pawluk
// test sonaru HC-SR04

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// konfiguracja LCD
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// konfiguracja sonaru
const uint8_t TRIG_PIN = A3;
const uint8_t ECHO_PIN = A2;
const unsigned long ECHO_TIMEOUT_US = 25000;
const unsigned int MIN_VALID_MM = 34;
const unsigned int MAX_VALID_MM = 4000;
const unsigned long MEASURE_MS = 100;

unsigned int minMm = 0xFFFF;
unsigned int maxMm = 0;

// mierzy odległość do przeszkody sonarem w mm
unsigned int measureMm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long us = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (us == 0) return 0;

  // dźwięk ~343 m/s -> 1 mm w obie strony = ~5.83 us
  unsigned int mm = (us * 100UL) / 583UL;
  if (mm < MIN_VALID_MM || mm > MAX_VALID_MM) return 0;
  return mm;
}

unsigned long prevMillis = 0;
void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void loop() {
  unsigned long now = millis();
  if (now - prevMillis < MEASURE_MS) return;
  prevMillis = now;

  unsigned int mm = measureMm();
  char line[17];
  lcd.setCursor(0,0);
  if (mm == 0) {
    snprintf(line, sizeof(line), "Brak echa       ");
  } else {
    if (mm < minMm) minMm = mm;
    if (mm > maxMm) maxMm = mm;
    snprintf(line, sizeof(line), "Odl: %4u.%u cm  ", mm / 10, mm % 10);
  }
  lcd.print(line);

  lcd.setCursor(0, 1);
  if (maxMm == 0) {
    snprintf(line, sizeof(line), "  -- -- cm");
  } else {
    snprintf(line, sizeof(line), "%4u-%4ucm", minMm / 10, maxMm / 10);
  }
  lcd.print(line);
}