// Błażej Pawluk
// obsługa sonaru HC-SR04

#pragma once
#include <Arduino.h>

extern unsigned int sonarMm; // ostatni pomiar w mm (0 - brak echa / brak pomiaru)

void initSonar();
void triggerSonar(); // wykonanie pomiaru (zapis do sonarMm)
