#include <WiFi.h>
#include <WebSocketsServer.h>

const char* WIFI_SSID = "aaa";
const char* WIFI_PASSWORD = "Blazej_16";

WebSocketsServer webSocket = WebSocketsServer(81);

unsigned long counter = 0;
unsigned long lastMessageTime = 0;

void webSocketEvent(
  uint8_t clientNum,
  WStype_t type,
  uint8_t* payload,
  size_t length
) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress clientIP = webSocket.remoteIP(clientNum);
      Serial.print("Klient polaczony: ");
      Serial.println(clientIP);
      break;
    }
    case WStype_DISCONNECTED:
      Serial.print("Klient rozlaczony: ");
      Serial.println(clientNum);
      break;
    case WStype_TEXT:
      Serial.print("Otrzymano od klienta: ");
      Serial.write(payload, length);
      Serial.println();
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32-CAM WebSocket");
  Serial.println("==============================");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Laczenie z WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi polaczone!");

  Serial.print("Adres IP ESP32-CAM: ");
  Serial.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket uruchomiony na porcie 81");
  Serial.println("==============================");
}

void loop() {
  webSocket.loop();

  unsigned long currentTime = millis();
  if (currentTime - lastMessageTime >= 2000) {
    lastMessageTime = currentTime;
    
    counter++;
    String message = "wiadomość " + String(counter);

    Serial.print("Wysylam: ");
    Serial.println(message);

    webSocket.broadcastTXT(message);
  }
}