/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka wykonująca polecenia trybu mapowania

  Mapę i wybór trasy obsługuje przeglądarka; Arduino wykonuje pojedyncze polecenia:
    - pomiar z postoju w zadanym kierunku (mediana z NAV_SCAN_PINGS pomiarów),
    - obrót w miejscu do zadanego kierunku (regulacja na podstawie żyroskopu),
    - jazdę na zadaną odległość z utrzymaniem kierunku; podczas jazdy do przodu sonar
      omiata przestrzeń przed pojazdem, a przeszkoda w korytarzu jazdy zatrzymuje pojazd

  Polecenie powtórzone z tym samym id nie jest wykonywane ponownie: podtrzymuje połączenie,
  a po zakończeniu polecenia powoduje ponowne wysłanie wyniku (A:NAV)
*/

#pragma once

#include <Arduino.h>
#include "position.h"

// wynik polecenia
enum NavResult : uint8_t {
  NAV_RUNNING = 0,
  NAV_DONE = 1,
  NAV_OBSTACLE = 2, // przeszkoda w korytarzu jazdy
  NAV_STALLED = 3,  // brak ruchu mimo zasilania silników
  NAV_ABORTED = 4,  // przerwano (W:NAVSTOP, inna komenda, utrata połączenia)
  NAV_BLIND = 5     // zbyt długa jazda bez echa z przodu
};

// wynik gotowy do wysłania jako A:NAV
struct NavReport {
  int id;
  NavResult result;
  int servo; // kąt serwa [stopnie]
  unsigned int mm; // wynik pomiaru z postoju (0 = brak echa)
  Pose pose;
};

// polecenia; zwracają false, jeśli to powtórzenie polecenia o tym samym id
bool navScan(int id, int angle);
bool navTurn(int id, int headingCdeg);
bool navDrive(int id, int mm, int headingCdeg);

// zatrzymuje pojazd i kończy bieżące polecenie wynikiem NAV_ABORTED
void navAbort();

// true, gdy wykonywane jest polecenie (sonar i serwo są wtedy zajęte)
bool navActive();

// obsługuje bieżące polecenie; wywoływać w każdym obiegu loop()
void navUpdate();

// przekazuje wynik pomiaru sonaru; zwraca true, jeśli pomiar należał do pomiaru z postoju
bool navOnMeasurement(unsigned int mm, int angle, const Pose& pose);

// zwraca true i wypełnia report, jeśli jest wynik do wysłania
bool navTakeReport(NavReport& report);
