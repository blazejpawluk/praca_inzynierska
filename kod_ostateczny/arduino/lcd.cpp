/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi wyświetlacza LCD (opis w lcd.h)
*/

#include "lcd.h"
#include <LiquidCrystal_I2C.h>

// konfiguracja
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_ROWS = 2;

static LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
static char shown[LCD_ROWS][LCD_COLS]; // aktualna zawartość wyświetlacza

void lcdInit() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  memset(shown, ' ', sizeof(shown));
}

// wspólna część lcdPrint: tekst z RAM (flash = false) lub z pamięci programu (flash = true)
static void printImpl(uint8_t row, const char* text, bool flash) {
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

void lcdPrint(uint8_t row, const char* text) {
  printImpl(row, text, false);
}

void lcdPrint(uint8_t row, const __FlashStringHelper* text) {
  printImpl(row, (const char*)text, true);
}
