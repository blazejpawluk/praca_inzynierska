// Błażej Pawluk
// obsługa LCD

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// konfiguracja LCD
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

void initLCD() {
  lcd.init();
  Wire.setClock(400000);
  lcd.backlight();
  lcd.clear();
}

const uint8_t TEXT_BUF_SIZE = 64;
char lastShown[TEXT_BUF_SIZE];
void printRow(uint8_t row, const char* text) {
  size_t len = strlen(text);
  lcd.setCursor(0, row);
  for (uint8_t col = 0; col < LCD_COLS; col++) {
    lcd.print(col < len ? text[col] : ' ');
  }
}

void showStatus(const char* row0, const char* row1) {
  lastShown[0] = '\0';
  printRow(0, row0);
  printRow(1, row1);
}

void showOnLCD(const char* text) {
  // pomija zapis takiego samego tekstu
  if (strcmp(text, lastShown) == 0) return;

  strncpy(lastShown, text, sizeof(lastShown) - 1);

  size_t len = strlen(text);
  for (uint8_t row = 0; row < LCD_ROWS; row++) {
    lcd.setCursor(0, row);
    for (uint8_t col = 0; col < LCD_COLS; col++) {
      size_t i = (size_t)row * LCD_COLS + col;
      lcd.print(i < len ? text[i] : ' '); // dopisywanie spacji zamiast robienia lcd.clear(), aby usunąć efekt migania
    }
  }
}