/*
 * TEST SONARU HC-SR04
 * TRIG -> A3
 * ECHO -> A2
 * Serwo (z sonarem) -> D9 - ustawiane na środek (90°), żeby sonar patrzył prosto
 * LCD I2C 16x2      -> SDA = A4, SCL = A5
 *
 * Ekran:
 *   Odl:  123.4 cm  *     <- bieżąca odległość ('*' miga przy każdym pomiarze)
 *     5- 230cm E  3       <- min-max od startu, E = liczba pomiarów bez echa
 *
 * Wymagana biblioteka: "LiquidCrystal I2C" (Frank de Brabander)
 * Servo.h jest wbudowana w Arduino IDE.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ---------------- KONFIGURACJA ----------------
const uint8_t LCD_ADDR = 0x27;            // jeśli ekran pusty, spróbuj 0x3F
const uint8_t TRIG_PIN = A3;              // zamień z ECHO_PIN, jeśli masz odwrotnie
const uint8_t ECHO_PIN = A2;
const uint8_t SERVO_PIN = 9;

const bool CENTER_SERVO = false;           // false = nie ruszaj serwa
const uint8_t SERVO_CENTER_DEG = 90;

const unsigned long ECHO_TIMEOUT_US = 25000; // ~4.3 m, dalej = brak echa
const unsigned int MIN_VALID_MM = 20;        // poniżej 2 cm HC-SR04 jest niewiarygodny
const unsigned int MAX_VALID_MM = 4000;      // powyżej 4 m też
const unsigned long MEASURE_MS = 100;        // odstęp między pomiarami (min. 60 ms)
// ----------------------------------------------

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
Servo servo;

unsigned int minMm = 0xFFFF;
unsigned int maxMm = 0;
unsigned int errors = 0;
bool blink = false;
unsigned long prevMillis = 0;

// Zwraca odległość w mm albo 0, gdy brak poprawnego echa
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

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Test sonaru");
  lcd.setCursor(0, 1);
  lcd.print(CENTER_SERVO ? "Serwo -> 90 st." : "Serwo pominiete");

  if (CENTER_SERVO) {
    servo.attach(SERVO_PIN);
    servo.write(SERVO_CENTER_DEG);
    delay(800);        // czas na dojechanie
    servo.detach();    // odłączenie: brak drgań i poboru prądu w trakcie testu
  }

  delay(1000);
  lcd.clear();
}

void loop() {
  unsigned long now = millis();
  if (now - prevMillis < MEASURE_MS) return;
  prevMillis = now;

  unsigned int mm = measureMm();
  blink = !blink;
  char line[17];

  lcd.setCursor(0, 0);
  if (mm == 0) {
    errors++;
    snprintf(line, sizeof(line), "Brak echa      %c", blink ? '*' : ' ');
  } else {
    if (mm < minMm) minMm = mm;
    if (mm > maxMm) maxMm = mm;
    snprintf(line, sizeof(line), "Odl: %4u.%u cm %c", mm / 10, mm % 10, blink ? '*' : ' ');
  }
  lcd.print(line);

  lcd.setCursor(0, 1);
  if (maxMm == 0) {
    snprintf(line, sizeof(line), "  -- -- cm E%4u", errors);
  } else {
    snprintf(line, sizeof(line), "%4u-%4ucm E%4u", minMm / 10, maxMm / 10, errors);
  }
  lcd.print(line);
}