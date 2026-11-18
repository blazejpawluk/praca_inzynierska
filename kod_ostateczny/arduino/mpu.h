/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca odbieranie danych z modułu MPU6050 z akcelerometrem i żyroskopem

  Połączenie: SDA=A4, SCL=A5

  Moduł jest zamontowany do góry nogami (obrót o 180 stopni wokół osi Y),
  dlatego znaki osi X i Z są odwracane, aby odczyty odpowiadały normalnemu ułożeniu
*/

#pragma once

#include <Arduino.h>

// okres próbkowania MPU6050 [us] (200 Hz)
const unsigned long MPU_SAMPLE_US = 5000;

// liczba próbek kalibracji
const uint16_t MPU_CALIB_SAMPLES = 400;

// odczyt przeskalowany do liczb całkowitych
struct MpuData {
  int16_t ax, ay, az; // przyspieszenie [mg]
  long gx, gy, gz; // prędkość kątowa [0.01 stopnia/s]
  int16_t temp; // temperatura [0.01 stopnia C]
};

// konfiguruje układ i kalibruje żyroskop (MPU_CALIB_SAMPLES próbek);
// zwraca false, jeśli układ nie odpowiada lub kalibracja się nie powiodła
bool mpuInit();

// resetuje i konfiguruje układ bez kalibracji (offset żyroskopu pozostaje bez zmian);
// używane po awarii I2C, gdy pojazd mógł być w ruchu
bool mpuConfigure();

bool mpuCalibrate(uint16_t samples);

// true, jeśli od ostatniego odczytu powstała nowa próbka
bool mpuDataReady();

bool mpuRead(MpuData& data);

// prędkość kątowa wokół osi Z [stopnie/s]
// używane w position.h, gdzie nie są potrzebne inne dane
bool mpuReadGz(float& gz);