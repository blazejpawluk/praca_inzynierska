/*
  AUTKO - ESP32-CAM (AI Thinker) - wersja ze zdjęciami (bez transmisji na żywo)

  - własna sieć WiFi (access point): SSID "AUTKO", hasło "password", adres 192.168.4.1
  - WebSocket na porcie 80:  ws://192.168.4.1:80/
      linia z UART (Arduino) -> wszyscy klienci WebSocket
      tekst z WebSocket      -> UART (Arduino), z dopisanym '\n'
  - zdjęcie (JPEG) na żądanie:  http://192.168.4.1:81/capture

  Zdjęcie jest robione tylko wtedy, gdy strona o nie poprosi. Dzięki temu kamera
  nie pracuje non-stop i nie blokuje WiFi ani mostu UART <-> WebSocket.

  POŁĄCZENIE UART Z ARDUINO (przez konwerter poziomów 3.3V <-> 5V):
      ESP32 GPIO13 (RX)  <-  Arduino D1 (TX)
      ESP32 GPIO14 (TX)  ->  Arduino D0 (RX)
      wspólna masa (GND)

  Arduino IDE:
    - pakiet "esp32 by Espressif" w wersji 2.0.17 (w 3.x kamera nie działała)
    - płytka: "AI Thinker ESP32-CAM", PSRAM: Enabled
    - biblioteka: "WebSockets" (Markus Sattler / arduinoWebSockets)
*/

#include <WiFi.h>
#include <WebSocketsServer.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"

// ---------- Konfiguracja ----------
const char* AP_SSID = "AUTKO";
const char* AP_PASS = "password";

#define UART_RX_PIN 13
#define UART_TX_PIN 14
#define UART_BAUD   115200      // musi być takie samo jak w Arduino

// Rozdzielczość zdjęcia: FRAMESIZE_QQVGA (160x120), QVGA (320x240), VGA (640x480),
// SVGA (800x600), XGA (1024x768), SXGA (1280x1024), UXGA (1600x1200).
// Większe zdjęcie = dłuższe robienie i wysyłanie (VGA zajmuje ok. pół sekundy).
#define CAM_FRAME_SIZE   FRAMESIZE_VGA
#define CAM_JPEG_QUALITY 12              // 10 (najlepsza) .. 63 (najgorsza)
#define CAM_XCLK_HZ      10000000        // przy braku problemów można podnieść do 20000000

// ---------- Piny kamery (AI Thinker) ----------
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

WebSocketsServer webSocket(80);

// ---------- Kamera ----------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = CAM_XCLK_HZ;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = CAM_FRAME_SIZE;
  config.jpeg_quality = CAM_JPEG_QUALITY;

  // Jeden bufor wystarczy: zdjęcia robimy pojedynczo, na żądanie.
  config.fb_count    = 1;
  config.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Blad kamery: 0x%x\n", err);
    return false;
  }
  return true;
}

// ---------- Zdjęcie (port 81, /capture) ----------
static esp_err_t captureHandler(httpd_req_t* req) {
  // Pierwsza klatka po przerwie może być stara (zrobiona wcześniej i czekająca w buforze),
  // więc ją odrzucamy i bierzemy następną - zdjęcie jest wtedy aktualne.
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);
  fb = esp_camera_fb_get();

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");

  if (!fb) {
    Serial.println("Zdjecie: brak klatki");
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "brak klatki", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=autko.jpg");
  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  Serial.printf("Zdjecie: %u B\n", (unsigned)fb->len);
  esp_camera_fb_return(fb);
  return res;
}

void startPhotoServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;

  httpd_uri_t captureUri = {
    .uri      = "/capture",
    .method   = HTTP_GET,
    .handler  = captureHandler,
    .user_ctx = NULL
  };

  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &captureUri);
  } else {
    Serial.println("Nie udalo sie uruchomic serwera zdjec");
  }
}

// ---------- WebSocket -> UART ----------
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_TEXT:
      if (length > 0 && length < 120) {
        Serial2.write(payload, length);
        Serial2.write('\n');
      }
      break;
    case WStype_CONNECTED:
      Serial.printf("WS klient %u polaczony\n", num);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("WS klient %u rozlaczony\n", num);
      break;
    default:
      break;
  }
}

// ---------- UART -> WebSocket ----------
void forwardUartToWs() {
  static char buf[128];
  static size_t len = 0;

  while (Serial2.available() > 0) {
    char c = (char)Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (len > 0) {
        buf[len] = '\0';
        webSocket.broadcastTXT(buf);
        len = 0;
      }
    } else if (len < sizeof(buf) - 1) {
      buf[len++] = c;
    } else {
      len = 0;   // zbyt długa linia (śmieci) - odrzucamy
    }
  }
}

void setup() {
  Serial.begin(115200);   // USB - tylko podgląd/debug
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  // Ciche logi: błędy po zamknięciu połączenia przez przeglądarkę są nieszkodliwe
  esp_log_level_set("httpd_txrx", ESP_LOG_NONE);
  esp_log_level_set("httpd_uri", ESP_LOG_NONE);
  esp_log_level_set("cam_hal", ESP_LOG_ERROR);

  Serial.printf("PSRAM: %s, wolne: %u B\n", psramFound() ? "TAK" : "NIE", (unsigned)ESP.getFreePsram());

  // Access point: stały adres 192.168.4.1, nie potrzeba routera
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);   // mniejsze opóźnienia
  Serial.print("AP: ");
  Serial.println(WiFi.softAPIP());

  // Gdy kamera nie wystartuje, most UART <-> WebSocket nadal działa
  if (initCamera()) {
    startPhotoServer();
    Serial.println("Kamera OK, zdjecie: http://192.168.4.1:81/capture");
  }

  webSocket.begin();
  webSocket.onEvent(onWsEvent);
  webSocket.enableHeartbeat(15000, 3000, 2);   // wykrywa martwe połączenia
}

void loop() {
  webSocket.loop();
  forwardUartToWs();
  delay(1);   // oddaje czas procesora innym zadaniom (WiFi, serwer zdjęć)
}
