/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Endpoint /capture serwera kamery (rejestrowany w camera.cpp)

  GET http://192.168.4.1:81/capture
    - 200: zdjęcie JPEG (świeża klatka - poprzednia z bufora jest odrzucana)
    - 503: brak klatki z kamery
*/

#pragma once

#include "esp_http_server.h"

// handler robienia zdjęcia
esp_err_t captureHandler(httpd_req_t* req);