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
  Wire.setWireTimeout(3000, true);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  syncLCDCache();
}

// wspólna część printLCD: tekst z RAM (flash = false) lub z pamięci programu (flash = true)
void printLCDImpl(uint8_t row, const char* text, bool flash) {
  bool finish = false;
  bool cursorReady = false;
  for (uint8_t col = 0; col < LCD_COLS; col++) {
    char c = ' '; // spacje zamiast lcd.clear(), aby uniknąć efektu migania
    if (!finish) {
      c = flash ? (char)pgm_read_byte(text + col) : text[col];
      if (c == '\0') {
        finish = true;
        c = ' ';
      }
    }

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

// tekst z RAM (np. odebrana komenda)
void printLCD(uint8_t row, const char* text) {
  printLCDImpl(row, text, false);
}

// stały tekst z pamięci programu: printLCD(0, F("TEKST")) – nie zajmuje RAM
void printLCD(uint8_t row, const __FlashStringHelper* text) {
  printLCDImpl(row, (const char*)text, true);
}