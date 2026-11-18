/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca komunikację przez UART z Arduino UNO

  Połączenie: UART RX=GPIO13, TX=GPIO14 (przez konwerter poziomów logicznych 3.3V-5V)

  Protokoły komunikacji:
    - linia A: z Arduino > wszyscy klienci WebSocketa (oprócz A:CONNECTION; tylko drukowalne znaki ASCII)
    - A:CONNECTION z Arduino - odpowiedź E:CONNECTION:<status>
    - E:CONNECTION:<status> - wysyłane także bez żądania przy każdej zmianie statusu i co 1 s
      [0=oczekiwanie na uruchomienie AccessPoint, 1=oczekiwanie na połączenie z komputerem, 2=połączono]
    - komenda W: z WebSocketa > Arduino z dopisanym '\n' (co najmniej 30 ms odstępu między komendami;
      z komend czekających na wysłanie zostaje tylko najnowsza danego rodzaju, np. W:MOTOR:)
*/

#pragma once

#include <Arduino.h>

// status połączenia z komputerem
enum ConnectionStatus : uint8_t {
  // oczekiwanie na uruchomienie AccessPoint
  CONN_WAIT_AP = 0,

  // oczekiwanie na połączenie z komputerem
  CONN_WAIT_CLIENT = 1,

  // połączono
  CONN_OK = 2
};

// uruchamia UART i wysyła do Arduino początkowy status CONN_WAIT_AP
// (po restarcie ESP32 Arduino mogło pozostać w stanie połączenia)
void arduinoInit();

// odbiera dostępne bajty z UART i obsługuje kompletne linie, wysyła komendy z kolejki
// i co 1 s powtarza status połączenia; wywoływać w każdym obiegu loop()
void arduinoUpdate();

// wysyła komendę W: do Arduino z dopisanym '\n' (od razu albo z kolejki, aby zachować odstęp między komendami);
// odrzuca komendy, które nie zaczynają się od "W:", nie mieszczą się w buforze Arduino lub zawierają znaki inne niż drukowalne ASCII
void arduinoSendCommand(const uint8_t* payload, size_t length);

// ustawia status połączenia i od razu powiadamia Arduino o zmianie
// (Arduino przy aktywnym połączeniu nie wysyła A:CONNECTION, więc o rozłączeniu musi dowiedzieć się od ESP32)
void arduinoSetConnectionStatus(ConnectionStatus status);