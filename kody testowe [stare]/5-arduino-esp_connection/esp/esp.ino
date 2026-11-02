#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-CAM";
const char* password = "haslo123";

WebServer server(80);

// Ostatnia kompletna wiadomość odebrana z UART
String serialData = "";

String serialBuffer = "";

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Wi-Fi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println(WiFi.softAPIP());

  // Strona główna
  server.on("/", HTTP_GET, []() {

    String html = "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>ESP32</title>";
    html += "</head>";

    html += "<body>";

    html += "<div id='dane'>";
    html += serialData;
    html += "</div>";

    html += "<script>";
    html += "setInterval(() => {";
    html += "fetch('/data')";
    html += ".then(r => r.text())";
    html += ".then(t => document.getElementById('dane').innerText = t);";
    html += "}, 100);";
    html += "</script>";

    html += "</body>";
    html += "</html>";

    server.send(200, "text/html; charset=UTF-8", html);
  });

  // Aktualne dane UART
  server.on("/data", HTTP_GET, []() {
    server.send(200, "text/plain; charset=UTF-8", serialData);
  });

  server.begin();
}

void loop() {

  // Obsługa strony WWW
  server.handleClient();

  // Odczyt danych z UART
  while (Serial.available()) {

    char c = Serial.read();

    // Koniec wiadomości
    if (c == '\n') {

      // Nowa wiadomość zastępuje poprzednią
      serialData = serialBuffer;

      // Przygotowanie bufora na następną wiadomość
      serialBuffer = "";
    }
    else {
      // Dodaj znak dokładnie taki, jaki przyszedł
      serialBuffer += c;
    }
  }
}