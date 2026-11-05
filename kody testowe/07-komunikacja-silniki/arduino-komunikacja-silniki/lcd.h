// Błażej Pawluk
// obsługa wyświetlacza LCD 16x2 (I2C)

#pragma once
#include <Arduino.h>

void initLCD();
void showStatus(const char* row0, const char* row1); // wyświetlenie dwóch osobnych linii
void showOnLCD(const char* text);                     // wyświetlenie tekstu zawiniętego na dwie linie
