// Błażej Pawluk
// wyliczanie pozycji i kąta autka: droga z enkoderów, kąt z żyroskopu, filtr Kalmana prędkości (enkodery + akcelerometr)
// układ współrzędnych ustalany przy starcie: X - przód, Y - lewo, kąt - skręt w lewo

#pragma once
#include <Arduino.h>

extern float posX, posY; // m
extern float heading;    // stopnie (-180, 180]
extern float vel;        // m/s, prędkość wzdłuż osi autka (ujemna - do tyłu)
extern bool slip;        // true - wykryty poślizg kół (enkodery niezgodne z akcelerometrem)
extern bool still;       // true - autko stoi (zerowanie prędkości i dryfu)

void initPosition();
void resetPosition();  // pozycja (0, 0), kąt 0
void updatePosition(); // wywoływane po każdej nowej próbce MPU (100 Hz)
