/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Strona interfejsu użytkownika (rejestrowana w camera.cpp)

  GET http://192.168.4.1:81/ - strona HTML przechowywana w pamięci programu (PROGMEM)
*/

#pragma once

#include "esp_http_server.h"

// handler wysyłający stronę
esp_err_t pageHandler(httpd_req_t* req);