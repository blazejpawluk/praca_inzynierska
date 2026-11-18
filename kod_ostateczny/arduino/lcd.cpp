/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi wyświetlacza LCD (opis w lcd.h)
*/

#include "lcd.h"
#include "i2c.h"
#include <LiquidCrystal_I2C.h>

// konfiguracja
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_ROWS = 2;

// ponowna synchronizacja trybu 4-bitowego (działa niezależnie od tego, na którym półbajcie urwano zapis)
const uint8_t LCD_SYNC_8BIT = 0x33;
const uint8_t LCD_SYNC_4BIT = 0x32;
const uint8_t LCD_SYNC_DELAY_MS = 5;

static LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
static char shown[LCD_ROWS][LCD_COLS]; // aktualna zawartość wyświetlacza

void lcdInit() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
  memset(shown, ' ', sizeof(shown));
}

void lcdRecover() {
  if (!i2cReady()) return;

  lcd.backlight();
  lcd.command(LCD_SYNC_8BIT);
  delay(LCD_SYNC_DELAY_MS);
  lcd.command(LCD_SYNC_8BIT);
  delay(LCD_SYNC_DELAY_MS);
  lcd.command(LCD_SYNC_4BIT);
  delay(LCD_SYNC_DELAY_MS);

  lcd.command(LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS);
  lcd.display();
  lcd.clear();
  lcd.command(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT);

  // odtworzenie zawartości, w tym zmian, których nie udało się zapisać podczas awarii
  for (uint8_t row = 0; row < LCD_ROWS; row++) {
    lcd.setCursor(0, row);
    for (uint8_t col = 0; col < LCD_COLS; col++) {
      if (!i2cReady()) return;
      lcd.write(shown[row][col]);
    }
  }
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

    // podczas awarii I2C znak jest tylko zapamiętywany; wyświetli go lcdRecover()
    shown[row][col] = c;
    if (!i2cReady()) continue;

    // aktualizacja tylko tego znaku, który się zmienił
    if (!cursorReady) {
      lcd.setCursor(col, row);
      cursorReady = true;
    }
    lcd.write(c);
  }
}

void lcdPrint(uint8_t row, const char* text) {
  printImpl(row, text, false);
}

void lcdPrint(uint8_t row, const __FlashStringHelper* text) {
  printImpl(row, (const char*)text, true);
}