/*
  AUTKO - ESP32-CAM (AI Thinker)

  - własna sieć WiFi (access point): SSID "AUTKO", hasło "password", adres 192.168.4.1
  - WebSocket na porcie 80:  ws://192.168.4.1:80/
      linia z UART (Arduino) -> wszyscy klienci WebSocket
      tekst z WebSocket      -> UART (Arduino), z dopisanym '\n'
  - strumień MJPEG na porcie 81:  http://192.168.4.1:81/stream

  POŁĄCZENIE UART Z ARDUINO (przez konwerter poziomów 3.3V <-> 5V):
      ESP32 GPIO13 (RX)  <-  Arduino D1 (TX)
      ESP32 GPIO14 (TX)  ->  Arduino D0 (RX)
      wspólna masa (GND)
  Piny kamery AI Thinker: 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39
  (GPIO16 zajmuje PSRAM). GPIO13 i 14 są wolne, nie są pinami rozruchowymi
  (strapping), a karta SD i tak nie jest używana.

  Arduino IDE:
    - płytka: "AI Thinker ESP32-CAM", PSRAM: Enabled
    - partycje: "Huge APP (3MB No OTA/1MB SPIFFS)" (gdyby brakło miejsca)
    - biblioteka: "WebSockets" (Markus Sattler / arduinoWebSockets)
    - esp_camera i esp_http_server są wbudowane w pakiet ESP32
*/

#include <WiFi.h>
#include <WebSocketsServer.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ---------- Konfiguracja ----------
const char* AP_SSID = "AUTKO";
const char* AP_PASS = "password";

#define UART_RX_PIN 13
#define UART_TX_PIN 14
#define UART_BAUD   115200      // musi być takie samo jak w Arduino

#define CAM_FRAME_SIZE   FRAMESIZE_QVGA   // 320x240; większa = większe opóźnienie (VGA = 640x480)
#define CAM_JPEG_QUALITY 12               // 10 (najlepsza) .. 63 (najgorsza)

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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = CAM_FRAME_SIZE;
  config.jpeg_quality = CAM_JPEG_QUALITY;

  if (psramFound()) {
    config.fb_count    = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode   = CAMERA_GRAB_LATEST;   // zawsze najnowsza klatka = mniejsze opóźnienie
  } else {
    config.fb_count    = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Blad kamery: 0x%x\n", err);
    return false;
  }
  return true;
}

// ---------- Strumień MJPEG (port 81) ----------
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
static const char* STREAM_BOUNDARY     = "\r\n--frame\r\n";
static const char* STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t streamHandler(httpd_req_t* req) {
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char partBuf[64];
  while (true) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, (unsigned)fb->len);
      res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
      if (res == ESP_OK) res = httpd_resp_send_chunk(req, partBuf, hlen);
      if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
      esp_camera_fb_return(fb);
    }
    if (res != ESP_OK) break;   // klient się rozłączył
    vTaskDelay(1);
  }
  return res;
}

void startStreamServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;

  httpd_uri_t streamUri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = streamHandler,
    .user_ctx = NULL
  };

  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &streamUri);
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

  // Access point: stały adres 192.168.4.1, nie potrzeba routera
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);   // mniejsze opóźnienia
  Serial.print("AP: ");
  Serial.println(WiFi.softAPIP());

  // Gdy kamera nie wystartuje, most UART <-> WebSocket nadal działa
  if (initCamera()) {
    startStreamServer();
  }

  webSocket.begin();
  webSocket.onEvent(onWsEvent);
  webSocket.enableHeartbeat(15000, 3000, 2);   // wykrywa martwe połączenia
}

void loop() {
  webSocket.loop();
  forwardUartToWs();
}
