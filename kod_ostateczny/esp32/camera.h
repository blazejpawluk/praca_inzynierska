/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca kamerę OV2640 i serwer HTTP do sterowania nią

  Serwer HTTP na porcie 81: http://192.168.4.1:81/
    - / - strona interfejsu użytkownika (page.h; dostępna także bez działającej kamery)
    - /capture - zdjęcie JPEG robione na żądanie (capture.h)
    - /stream/start, /stream, /stream/stop - transmisja na żywo (stream.h)
  (transmisja jest domyślnie wyłączona z powodu dużego obciążenia zasilania - zdjęcie na żądanie jest lżejsze)

  Oszczędzanie energii: nieużywana kamera jest usypiana (wyłączenie sterownika i PWDN=1),
  a budzona przy pierwszym pobraniu klatki (ok. 0.5 s opóźnienia: inicjalizacja i ustalenie ekspozycji).
*/

#pragma once

#include "esp_camera.h"
#include "esp_http_server.h"

// sprawdza działanie kamery (inicjalizacja) i usypia ją; zwraca false przy błędzie
bool cameraInit();

// usypia kamerę, jeśli przez CAM_IDLE_TIMEOUT_MS nikt nie pobierał klatek; wywoływać w każdym obiegu loop()
void cameraUpdate();

// pobiera klatkę (w razie potrzeby budząc kamerę); zwraca NULL przy błędzie
// każdą pobraną klatkę trzeba oddać przez cameraReturnFrame()
camera_fb_t* cameraGetFrame();

// oddaje klatkę pobraną przez cameraGetFrame()
void cameraReturnFrame(camera_fb_t* fb);

// uruchamia serwer HTTP i rejestruje endpointy z CAMERA_ENDPOINTS
bool cameraServerStart();

// ustawia nagłówki wspólne dla odpowiedzi wszystkich endpointów (CORS, brak cache)
void cameraSetCommonHeaders(httpd_req_t* req);