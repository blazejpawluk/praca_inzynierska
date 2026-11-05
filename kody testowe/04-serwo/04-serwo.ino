// Błażej Pawluk
// test serwomechanizmu

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// konfiguracja LCD
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// konfiguracja serwo
const uint8_t SERVO_PIN = 9;
const int CENTER_US = 1500;
const int MAX_ANGLE = 45;
const float US_PER_DEG = 1856.0 / 180.0;
const unsigned long STEP_MS = 15;
int angle = 0; 
int sweepDir = 1;
Servo servo;

int angleToUs(int a) {
  return CENTER_US - (int)lround(a * US_PER_DEG);
}

void moveTo(int a) {
  angle = constrain(a, -MAX_ANGLE, MAX_ANGLE);
  servo.writeMicroseconds(angleToUs(angle));
}

unsigned long lastMoveMs = 0;
void runSweep(unsigned long now) {
  if (now - lastMoveMs < STEP_MS) return;
  lastMoveMs = now;
  
  if (angle >= MAX_ANGLE)  sweepDir = -1;
  if (angle <= -MAX_ANGLE) sweepDir = 1;
  moveTo(angle + sweepDir);

  lcd.setCursor(0,0);
  char line[LCD_COLS + 1];
  snprintf(line, sizeof(line), "kat: %10u", 90 - angle);
  lcd.print(line);
}

void setup() {
  servo.attach(SERVO_PIN);
  servo.writeMicroseconds(CENTER_US);
  delay(1500);
  lcd.clear();

  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void loop() {
  unsigned long now = millis();
  runSweep(now);
}