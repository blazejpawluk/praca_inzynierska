// Błażej Pawluk
// komunikacja przez UART z ESP32-CAM: odbieranie poleceń i wysyłanie danych

#pragma once
#include <Arduino.h>

void readSerial(); // odczytywanie i wykonywanie poleceń
void sendData();   // zbieranie danych z sensorów i wysyłanie
