// Błażej Pawluk
// obsługa żyroskopu i akcelerometru (MPU-6050)
// akcelerometr: x - przód, y - lewo, z - góra
// żyroskop: x - prawo w dół, y - przód w dół, z - skręt w lewo

#pragma once
#include <Arduino.h>

extern float ax, ay, az; // g
extern float gx, gy, gz; // stopnie/s (po odjęciu offsetu)
extern float tempC;

bool initMPU();
void calibrateGyro();           // robot musi stać nieruchomo
bool updateMPU(unsigned long now); // odczyt co SAMPLE_MS, true - nowa próbka
