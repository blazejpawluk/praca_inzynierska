// Błażej Pawluk
// test enkoderów szczelinowych kół

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// konfiguracja LCD
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

const unsigned long DEBOUNCE_US = 2000;
const unsigned long REFRESH_MS = 500;

// piny enkoderów
const uint8_t ENL_PIN = A0; // PC0, PCINT8
const uint8_t ENR_PIN = A1; // PC1, PCINT9

// liczniki
volatile unsigned long enL = 0;
volatile unsigned long enR = 0;
volatile unsigned long lastL = 0;
volatile unsigned long lastR = 0;
volatile uint8_t prevState = 0;

// przerwanie pin change
ISR(PCINT1_vect) {
  uint8_t state = PINC & 0b00000011; // bit0 = A0, bit1 = A1
  uint8_t rising = state & ~prevState; // tylko zmiana z 0 na 1
  prevState = state;
  unsigned long now = micros();

  if ((rising & 0b01) && (now - lastL > DEBOUNCE_US)) {
    enL++;
    lastL = now;
  }
  if ((rising & 0b10) && (now - lastR > DEBOUNCE_US)) {
    enR++;
    lastR = now;
  }
}

unsigned long prevMillis = 0;
void setup() {
  pinMode(ENL_PIN, INPUT);
  pinMode(ENR_PIN, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  // włączenie przerwań
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
  prevMillis = nowMs;

  noInterrupts();
  unsigned long l = enL, r = enR;
  interrupts();

  char line[LCD_COLS + 1];
  snprintf(line, sizeof(line), "L: %10lu", l);
  lcd.setCursor(0, 0); lcd.print(line);
  snprintf(line, sizeof(line), "R: %10lu", r);
  lcd.setCursor(0, 1); lcd.print(line);
}