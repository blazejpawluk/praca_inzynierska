/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca wyświetlacz LCD 16x2 przez I2C

  Połączenie: SDA=A4, SCL=A5
*/

#pragma once

#include <Arduino.h>

const uint8_t LCD_COLS = 16;

// przeznaczenie wierszy
const uint8_t LCD_ROW_STATUS = 0; // stan inicjalizacji lub połączenia / pierwsza linia W:MSG
const uint8_t LCD_ROW_INFO = 1; // komunikat pomocniczy / druga linia W:MSG

void lcdInit();

// przywraca stan wyświetlacza i jego zawartość po awarii I2C (bez długich opóźnień lcdInit)
void lcdRecover();

// wypisuje tekst w wierszu (dopełniony spacjami, przycięty do LCD_COLS);
// zapisywane są tylko znaki, które się zmieniły
void lcdPrint(uint8_t row, const char* text); // tekst z RAM (np. odebrany komunikat)
void lcdPrint(uint8_t row, const __FlashStringHelper* text); // stały tekst: lcdPrint(0, F("TEKST"))