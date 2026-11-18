/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca komunikację wychodzącą przez UART do ESP32-CAM

  Połączenie: UART TX=D1 (do ESP32-CAM przez konwerter poziomów logicznych z 5V na 3.3V)

  Protokoły komunikacji wychodzącej z Arduino:
    - A:ENC:<left>,<right> - enkodery kół [liczba impulsów, 2 na szczelinę]
    - A:DIST:<angle>,<distance>,<x>,<y>,<theta> - odległość od przeszkody i poza pojazdu w chwili wyzwolenia pomiaru [stopnie, mm, mm, mm, 0.01 stopnia] (liczby całkowite)
    - A:MPU:<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<temp> - MPU6050 [mg, 0.01 stopnia/s, 0.01 stopnia Celsjusza] (liczby całkowite)
    - A:POS:<x>,<y>,<angle>,<velocity> - pozycja [mm, mm, 0.01 stopnia, mm/s] (liczby całkowite)
    - A:CONNECTION - żądanie statusu połączenia z komputerem

  Dane są wysyłane w cyklach (ENC, DIST, MPU, POS) po jednej wiadomości na obieg loop(),
  tylko gdy mieszczą się w buforze nadawczym, więc wysyłanie nie blokuje programu.
*/

#pragma once

#include "position.h"

// rozpoczyna nowy cykl wysyłania (przy wyłączonym radarze także pomiar sonaru);
// pomija, jeśli poprzedni cykl jeszcze trwa
void commStartSendCycle();

// porzuca niewysłane wiadomości (np. po utracie połączenia)
void commResetSendCycle();

// wysyła najwyżej jedną wiadomość; wywoływać w każdym obiegu loop()
void commSendNext();

// zapamiętuje wynik pomiaru sonaru do wysłania jako A:DIST
void commQueueDist(int angle, unsigned int mm, const Pose& pose);

// wysyła A:CONNECTION najwyżej co CONNECTION_INTERVAL_MS (bez blokowania)
void commRequestConnection();