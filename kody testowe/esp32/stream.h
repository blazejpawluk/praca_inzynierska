/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Endpointy transmisji na żywo z kamery (rejestrowane w camera.cpp)

  GET http://192.168.4.1:81/stream/start - włączenie transmisji
  GET http://192.168.4.1:81/stream - odbieranie transmisji (MJPEG: multipart/x-mixed-replace, np. w <img src="...">)
    - 409: transmisja wyłączona
  GET http://192.168.4.1:81/stream/stop - wyłączenie transmisji i rozłączenie odbiorcy

  Transmisja jest domyślnie wyłączona z powodu dużego obciążenia zasilania.
  Odbiorca może być tylko jeden - nowe żądanie /stream rozłącza poprzedniego.
  Klatki są wysyłane przez osobne zadanie FreeRTOS, więc serwer może w tym czasie obsługiwać
  inne endpointy (np. /capture, /stream/stop).
*/

#pragma once

#include "esp_http_server.h"

// tworzy zadanie wysyłające klatki (wywoływać przed uruchomieniem serwera HTTP); zwraca false przy błędzie
bool streamInit();

// powiadamia o zamknięciu gniazda przez serwer HTTP (wywoływać w close_fn serwera przed close())
void streamOnSocketClose(int fd);

// handler włączenia transmisji
esp_err_t streamStartHandler(httpd_req_t* req);

// handler odbierania transmisji
esp_err_t streamHandler(httpd_req_t* req);

// handler wyłączenia transmisji
esp_err_t streamStopHandler(httpd_req_t* req);