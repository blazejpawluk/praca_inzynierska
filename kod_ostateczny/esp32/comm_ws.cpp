/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja komunikacji przez WebSocket (opis i protokół w comm_ws.h)
*/

#include <WebSocketsServer.h>
#include "comm_ws.h"
#include "comm_arduino.h"

// konfiguracja WebSocketa
const uint16_t WS_PORT = 80;
const uint32_t WS_PING_INTERVAL_MS = 15000;
const uint32_t WS_PONG_TIMEOUT_MS = 3000;
const uint8_t WS_DISCONNECT_PONG_COUNT = 2;

static WebSocketsServer webSocket(WS_PORT);

static void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) arduinoSendCommand(payload, length);
}

void wsInit() {
  webSocket.begin();
  webSocket.onEvent(onWsEvent);
  webSocket.enableHeartbeat(WS_PING_INTERVAL_MS, WS_PONG_TIMEOUT_MS, WS_DISCONNECT_PONG_COUNT);
}

void wsLoop() {
  webSocket.loop();
}

void wsBroadcast(const char* text) {
  // nieudane wysłanie oznacza zatrzymane połączenie TCP; bez rozłączenia każda kolejna linia
  // blokowałaby loop() na czas timeoutu, a strona i tak nie dostałaby danych
  for (uint8_t num = 0; num < WEBSOCKETS_SERVER_CLIENT_MAX; num++) {
    if (webSocket.clientIsConnected(num) && !webSocket.sendTXT(num, text)) webSocket.disconnect(num);
  }
}

bool wsHasClients() {
  return webSocket.connectedClients() > 0;
}