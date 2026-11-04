/*
 * TEST ENKODERÓW KÓŁ
 * Lewy enkoder  (DO) -> A1
 * Prawy enkoder (DO) -> A0
 * LCD I2C 16x2       -> SDA = A4, SCL = A5
 *
 * Silniki nie są używane - obracaj koła ręcznie.
 *
 * Ekran:
 *   L 000123 R 000118 x   <- liczba impulsów od startu + stan pinu lewego (0/1)
 *   L  45 R  44 rpm   y   <- obroty/min koła + stan pinu prawego (0/1)
 *
 * Wymagana biblioteka: "LiquidCrystal I2C" (Frank de Brabander)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------------- KONFIGURACJA ----------------
const uint8_t LCD_ADDR = 0x27;          // jeśli ekran pusty, spróbuj 0x3F
const uint8_t ENC_L_PIN = A1;           // PC1 / PCINT9
const uint8_t ENC_R_PIN = A0;           // PC0 / PCINT8

const uint8_t SLOTS = 20;               // liczba szczelin w tarczy enkodera
const unsigned long DEBOUNCE_US = 2000; // filtr drgań styku (2 ms)
const unsigned long REFRESH_MS = 500;   // odświeżanie LCD i pomiar rpm
// ----------------------------------------------

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

volatile unsigned long countL = 0;
volatile unsigned long countR = 0;
volatile unsigned long lastL = 0;
volatile unsigned long lastR = 0;
volatile uint8_t prevState = 0;

// Przerwanie Pin Change dla portu C (A0..A5).
// UNO ma przerwania zewnętrzne tylko na D2/D3, więc dla A0/A1 używamy PCINT.
ISR(PCINT1_vect) {
  uint8_t state = PINC & 0b00000011;    // bit0 = A0, bit1 = A1
  uint8_t rising = state & ~prevState;  // tylko zbocza narastające
  prevState = state;
  unsigned long now = micros();

  if ((rising & 0b01) && (now - lastL > DEBOUNCE_US)) {
    countL++;
    lastL = now;
  }
  if ((rising & 0b10) && (now - lastR > DEBOUNCE_US)) {
    countR++;
    lastR = now;
  }
}

unsigned long prevCountL = 0;
unsigned long prevCountR = 0;
unsigned long prevMillis = 0;

void setup() {
  pinMode(ENC_L_PIN, INPUT);
  pinMode(ENC_R_PIN, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Test enkoderow");
  lcd.setCursor(0, 1);
  lcd.print("Krec kolami...");
  delay(1500);
  lcd.clear();

  // Włączenie przerwań Pin Change dla A0 i A1
  noInterrupts();
  prevState = PINC & 0b00000011;
  PCICR  |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9);
  interrupts();

  prevMillis = millis();
}

void loop() {
  unsigned long nowMs = millis();
  if (nowMs - prevMillis < REFRESH_MS) return;

  unsigned long dt = nowMs - prevMillis;
  prevMillis = nowMs;

  // Atomowe skopiowanie liczników
  noInterrupts();
  unsigned long cL = countL;
  unsigned long cR = countR;
  interrupts();

  unsigned long dL = cL - prevCountL;
  unsigned long dR = cR - prevCountR;
  prevCountL = cL;
  prevCountR = cR;

  // rpm = impulsy / szczeliny / czas[min]
  unsigned int rpmL = (dL * 60000UL) / ((unsigned long)SLOTS * dt);
  unsigned int rpmR = (dR * 60000UL) / ((unsigned long)SLOTS * dt);

  uint8_t pinL = digitalRead(ENC_L_PIN);
  uint8_t pinR = digitalRead(ENC_R_PIN);

  char line[17];

  snprintf(line, sizeof(line), "L%6lu R%6lu %u", cL % 1000000UL, cR % 1000000UL, pinL);
  lcd.setCursor(0, 0);
  lcd.print(line);

  snprintf(line, sizeof(line), "L%4u R%4u rpm%u", rpmL, rpmR, pinR);
  lcd.setCursor(0, 1);
  lcd.print(line);
}