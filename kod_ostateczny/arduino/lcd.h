/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca wyświetlacz LCD 16x2 przez I2C

  Połączenie: SDA=A4, SCL=A5
*/

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// konfiguracja
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
char shown[LCD_ROWS][LCD_COLS];

void syncLCDCache() {
  memset(shown, ' ', sizeof(shown));
}

void initLCD() {
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(3000, true);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  syncLCDCache();
}

void printLCD(uint8_t row, const char* text) {
  bool finish = false;
  bool cursorReady = false;
  for (uint8_t col = 0; col < LCD_COLS; col++) {
    if (!finish && text[col] == '\0') finish = true;
    char c = finish ? ' ' : text[col]; // spacje zamiast lcd.clear(), aby uniknąć efektu migania

    // jeśli znak się nie zmienia to pomijamy
    if (c == shown[row][col]) {
      cursorReady = false;
      continue;
    }

    // aktualizacja tylko tego znaku, który się zmienił
    if (!cursorReady) {
      lcd.setCursor(col, row);
      cursorReady = true;
    }
    lcd.write(c);
    shown[row][col] = c;
  }
}