// Błażej Pawluk
// obsługa enkoderów szczelinowych kół

#pragma once
#include <Arduino.h>

void initEncoders();
void readEncoders(unsigned long& l, unsigned long& r); // bezpieczny odczyt liczników
