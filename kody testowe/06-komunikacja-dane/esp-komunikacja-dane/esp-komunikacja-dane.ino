// Błażej Pawluk
// plik komunikacji przez UART z Arduino UNO i przez Access Point ze stroną internetową (bez zmian względem 01-komunikacja)

// połączenie z Arduino UNO (przez 2-kierunkowy konwerter poziomów):
// IO14 - D0 (RX)
// IO13 - D1 (TX)

// połączenie ze stroną internetową
// sieć Wi-Fi (tworzona przez ESP32-CAM): SSID: AUTKO, hasło: password, adres: 192.168.4.1
//  WebSocket na porcie 80: ws://192.168.4.1:80/
//    linia otrzymana przez Serial UART > wszyscy klienci WebSocketa
//    tekst z WebSocketa > Serial UART z dopisanym '/n'
//  WebSocket na porcie 81: ws://192.168.4.1:81/
//    zdjęcie robione na żądanie (http://192.168.4.1:81/capture)
// (możliwość robienia zdjęcia na żądanie zamiast nadawania obrazu na żywo z powodu zbyt dużego obciążenia zasilania)

#include <WiFi.h>
#include <WebSocketsServer.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_log.h"

// konfiguracja AccessPoint
const char* AP_SSID = "AUTKO";
const char* AP_PASS = "password";

// konfiguracja UART
#define UART_RX_PIN 13
#define UART_TX_PIN 14
const unsigned long UART_BAUD = 38400;

// konfiguracja kamery
#define CAM_FRAME_SIZE   FRAMESIZE_VGA // rozdzielczosc (640x480)
#define CAM_JPEG_QUALITY 12            // 10-63 (niższa - lepsza)
#define CAM_XCLK_HZ      20000000      // częstotliwość sygnału zegarowego
// piny kamery
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

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

  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Blad kamery: 0x%x\n", err);
    return false;
  }
  return true;
}

// handler robienia zdjęcia
static esp_err_t captureHandler(httpd_req_t* req) {
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

// port 81: do obsługi kamery
void startPhotoServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;

  httpd_uri_t captureUri = {
    .uri = "/capture",
    .method = HTTP_GET,
    .handler = captureHandler,
    .user_ctx = NULL
  };

  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &captureUri);
  } else {
    Serial.println("Nie udalo sie uruchomic serwera zdjec");
  }
}

// WebSocket
WebSocketsServer webSocket(80);

// obsługa przesyłania sygnałów
// WebSocket > UART
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

// UART > WebSocket
const uint8_t RX_BUF_SIZE = 64;
void forwardUartToWs() {
  static char buf[RX_BUF_SIZE];
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
  Serial.begin(115200); // podgląd / debug
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN); // Serial UART

  // ignorowane błędy
  esp_log_level_set("httpd_txrx", ESP_LOG_NONE);
  esp_log_level_set("httpd_uri", ESP_LOG_NONE);
  esp_log_level_set("cam_hal", ESP_LOG_ERROR);

  // Access point: 192.168.4.1
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);
  Serial.print("AP: ");
  Serial.println(WiFi.softAPIP());

  if (initCamera()) startPhotoServer();

  webSocket.begin();
  webSocket.onEvent(onWsEvent);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  webSocket.loop();
  forwardUartToWs();
  delay(1); // oddanie czasu procesora na Wi-Fi i zdjęcia
}