/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Kod na mikrokontroler ESP32-CAM

  Podłączone komponenty:
    - Arduino UNO
      połączenie UART: RX=GPIO13, TX=GPIO14 (przez konwerter poziomów logicznych 3.3V-5V)

  Połączenie z komputerem:
    sieć Wi-Fi (tworzona przez ESP32-CAM): SSID: AUTKO, hasło: password, adres: 192.168.4.1
    WebSocket na porcie 80: ws://192.168.4.1:80/
    serwer HTTP kamery na porcie 81: http://192.168.4.1:81/ (endpointy w camera.h)
*/

#include <WiFi.h>
#include "comm_arduino.h"
#include "comm_ws.h"
#include "camera.h"

// konfiguracja AccessPoint
const char* const AP_SSID = "AUTKO";
const char* const AP_PASS = "password";

void setup() {
  arduinoInit();

  // Access point: 192.168.4.1
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(AP_SSID, AP_PASS)) {
    while (true) {
      arduinoUpdate(); // odpowiadanie na A:CONNECTION statusem CONN_WAIT_AP
      delay(1);
    }
  }
  WiFi.setSleep(false);

  if (cameraInit()) cameraServerStart();

  wsInit();
  arduinoSetConnectionStatus(CONN_WAIT_CLIENT);
}

void loop() {
  wsLoop();
  arduinoSetConnectionStatus(wsHasClients() ? CONN_OK : CONN_WAIT_CLIENT);
  arduinoUpdate();
  cameraUpdate();
  delay(1); // oddanie czasu procesora na Wi-Fi
}