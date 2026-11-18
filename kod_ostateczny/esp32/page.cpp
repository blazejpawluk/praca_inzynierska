/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja strony interfejsu użytkownika (opis w page.h)
*/

#include <Arduino.h>
#include "camera.h"
#include "page.h"

// strona HTML w pamięci programu; na ESP32 pamięć flash jest mapowana w przestrzeni adresowej,
// więc tablicę można przekazać bezpośrednio do httpd_resp_send() bez kopiowania do RAM
static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Autko</title>
  </head>
  
  <body>
    <h1>Strona działa</h1>
  </body>
</html>
)rawliteral";

esp_err_t pageHandler(httpd_req_t* req) {
  cameraSetCommonHeaders(req);
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, PAGE_HTML, sizeof(PAGE_HTML) - 1); // bez kończącego '\0'
}
