/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca komunikację przychodzącą przez UART z ESP32-CAM

  Połączenie: UART RX=D0 (z ESP32-CAM przez konwerter poziomów logicznych z 3.3V na 5V)

  Protokoły komunikacji przychodzącej na Arduino:
    - W:SERVO:<angle> - ustawienie kąta SG90 [stopnie]
    - W:MOTOR:<left>,<right> - ustawienie prędkości silników [PWM]
    - W:RESETPOS - reset pozycji do (0,0,0)
    - W:RADAR:<from>,<to>,<step>,<interval> - uruchomienie radaru [stopnie, stopnie, stopnie, ms]
    - W:RADAROFF - wyłączenie radaru
    - W:NAVSCAN:<id>,<angle> - mapowanie: pomiar z postoju [-, stopnie]
    - W:NAVTURN:<id>,<heading> - mapowanie: obrót w miejscu do kierunku [-, 0.01 stopnia]
    - W:NAVGO:<id>,<distance>,<heading> - mapowanie: jazda z utrzymaniem kierunku [-, mm (ujemna = do tyłu), 0.01 stopnia]
    - W:NAVSTOP - mapowanie: przerwanie polecenia
    - W:MSG:<text> - tekst do wyświetlenia na LCD (maksymalnie 16 znaków)
    - W:CALIBRATE - kalibracja żyroskopu
    - E:CONNECTION:<status> - status połączenia z komputerem [0=oczekiwanie na uruchomienie AccessPoint, 1=oczekiwanie na połączenie z komputerem, 2=połączono]

  Komendy W: są wykonywane tylko przy statusie CONN_OK.
*/

#pragma once

#include <Arduino.h>

// status połączenia z komputerem (E:CONNECTION:<status>)
enum ConnectionStatus : uint8_t {
  // oczekiwanie na uruchomienie AccessPoint
  CONN_WAIT_AP = 0,

  // oczekiwanie na połączenie z komputerem
  CONN_WAIT_CLIENT = 1,
  
  // połączono
  CONN_OK = 2
};

// aktualny status połączenia (tylko do odczytu poza comm_in.cpp)
extern ConnectionStatus connectionStatus;

// odbiera dostępne bajty z UART i wykonuje kompletne komendy
void commReceive();

// wyświetla ostatnią komendę w wierszu LCD_ROW_CMD (najwyżej co LCD_REFRESH_MS)
void commShowLastCommand();

// porzuca komendę czekającą na wyświetlenie
void commClearLastCommand();

// zatrzymuje silniki, jeśli przez CMD_TIMEOUT_MS nie przyszła żadna komenda W:
// (frontend podczas jazdy powinien powtarzać W:MOTOR co ~500 ms)
void commCheckTimeout();