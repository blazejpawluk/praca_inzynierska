/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca i sterująca radarem złożonym z serwomechanizmu SG90 i sonaru ultradźwiękowego HC-SR04

  Radar przestawia serwo w zakresie [from, to] co step stopni; w każdym położeniu
  czeka interval ms, wykonuje pomiar i dopiero po jego wyniku przechodzi do kolejnego kąta
*/

#pragma once

// czy radar jest włączony (tylko do odczytu poza radar.cpp)
extern bool radarOn;

void radarStart(int from, int to, int step, long intervalMs);
void radarStop();

// porzuca oczekiwanie na pomiar (np. po utracie połączenia); radar pozostaje włączony
void radarPause();

// po upływie intervalMs rozpoczyna pomiar
void radarUpdate();

// wywoływać po otrzymaniu wyniku pomiaru sonaru: jeśli radar na niego czekał, przestawia serwo na kolejny kąt
void radarOnMeasurement();