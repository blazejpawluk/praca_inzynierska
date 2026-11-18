/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Obsługa magistrali I2C wspólnej dla LCD i MPU6050: timeout oraz odblokowanie po awarii

  Timeout biblioteki Wire resetuje tylko kontroler I2C w Arduino. Układ podrzędny przerwany
  w połowie bajtu może dalej trzymać SDA w stanie niskim, dlatego magistralę trzeba
  odblokować impulsami SCL i warunkiem STOP.

  Połączenie: SDA=A4, SCL=A5
*/

#pragma once

#include <Arduino.h>

// odblokowuje magistralę i uruchamia Wire
void i2cInit();

// false po timeoucie lub zgłoszonej awarii; do czasu odzyskania magistrali nie należy jej używać
bool i2cReady();

// zgłasza awarię wykrytą poza biblioteką Wire
void i2cMarkFault();

// odblokowuje magistralę i ponownie uruchamia Wire; zwraca true, jeśli SDA i SCL są wolne
bool i2cRecover();
