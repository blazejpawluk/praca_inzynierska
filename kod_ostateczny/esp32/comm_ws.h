/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca komunikację przez WebSocket ze stroną internetową

  WebSocket na porcie 80: ws://192.168.4.1:80/
    - tekst z WebSocketa > Arduino (tylko komendy W:, filtrowanie w comm_arduino.h)
    - linia z Arduino > wszyscy klienci WebSocketa
*/

#pragma once

#include <Arduino.h>

// uruchamia serwer WebSocket (wywoływać po uruchomieniu AccessPoint)
void wsInit();

// obsługuje serwer WebSocket; wywoływać w każdym obiegu loop()
void wsLoop();

// wysyła tekst do wszystkich połączonych klientów; klient, do którego wysłanie się nie powiedzie, jest rozłączany
void wsBroadcast(const char* text);

// zwraca true, jeśli połączony jest co najmniej jeden klient
bool wsHasClients();