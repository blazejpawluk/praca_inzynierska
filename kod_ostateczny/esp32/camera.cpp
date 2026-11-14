/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi kamery i serwera HTTP (opis w camera.h)
*/

#include <Arduino.h>
#include "esp_log.h"
#include "lwip/sockets.h"
#include "camera.h"
#include "capture.h"
#include "stream.h"

// konfiguracja kamery
const framesize_t CAM_FRAME_SIZE = FRAMESIZE_VGA; // rozdzielczość (640x480)
const int CAM_JPEG_QUALITY = 12; // 10-63 (niższa - lepsza)
const int CAM_XCLK_HZ = 20000000; // częstotliwość sygnału zegarowego

// piny kamery (ESP32-CAM AI-Thinker)
const int PWDN_GPIO_NUM  = 32;
const int RESET_GPIO_NUM = -1; // niepodłączony
const int XCLK_GPIO_NUM  = 0;
const int SIOD_GPIO_NUM  = 26;
const int SIOC_GPIO_NUM  = 27;
const int Y9_GPIO_NUM    = 35;
const int Y8_GPIO_NUM    = 34;
const int Y7_GPIO_NUM    = 39;
const int Y6_GPIO_NUM    = 36;
const int Y5_GPIO_NUM    = 21;
const int Y4_GPIO_NUM    = 19;
const int Y3_GPIO_NUM    = 18;
const int Y2_GPIO_NUM    = 5;
const int VSYNC_GPIO_NUM = 25;
const int HREF_GPIO_NUM  = 23;
const int PCLK_GPIO_NUM  = 22;

// oszczędzanie energii
const unsigned long CAM_IDLE_TIMEOUT_MS = 10000; // czas bez pobierania klatek, po którym kamera jest usypiana
const uint8_t CAM_WARMUP_FRAMES = 5; // klatki odrzucane po obudzeniu (ustalanie ekspozycji)

// konfiguracja serwera HTTP
const uint16_t CAM_SERVER_PORT = 81;

struct CameraEndpoint {
  const char* uri;
  httpd_method_t method;
  esp_err_t (*handler)(httpd_req_t* req);
};

// endpointy serwera HTTP
static const CameraEndpoint CAMERA_ENDPOINTS[] = {
  {"/capture", HTTP_GET, captureHandler},
  {"/stream/start", HTTP_GET, streamStartHandler},
  {"/stream", HTTP_GET, streamHandler},
  {"/stream/stop", HTTP_GET, streamStopHandler},
};
const size_t CAMERA_ENDPOINT_COUNT = sizeof(CAMERA_ENDPOINTS) / sizeof(CAMERA_ENDPOINTS[0]);

// stan zasilania kamery; chroniony przez cameraMutex (klatki pobiera serwer HTTP i zadanie transmisji, usypia loop())
static SemaphoreHandle_t cameraMutex = NULL;
static camera_config_t cameraConfig = {};
static bool cameraAwake = false;
static uint8_t framesInUse = 0; // liczba pobranych, a jeszcze nieoddanych klatek (oraz trwających pobrań)
static unsigned long lastFrameMs = 0;

static httpd_handle_t httpServer = NULL;

static void buildCameraConfig() {
  cameraConfig.ledc_channel = LEDC_CHANNEL_0;
  cameraConfig.ledc_timer   = LEDC_TIMER_0;
  cameraConfig.pin_d0 = Y2_GPIO_NUM;
  cameraConfig.pin_d1 = Y3_GPIO_NUM;
  cameraConfig.pin_d2 = Y4_GPIO_NUM;
  cameraConfig.pin_d3 = Y5_GPIO_NUM;
  cameraConfig.pin_d4 = Y6_GPIO_NUM;
  cameraConfig.pin_d5 = Y7_GPIO_NUM;
  cameraConfig.pin_d6 = Y8_GPIO_NUM;
  cameraConfig.pin_d7 = Y9_GPIO_NUM;
  cameraConfig.pin_xclk  = XCLK_GPIO_NUM;
  cameraConfig.pin_pclk  = PCLK_GPIO_NUM;
  cameraConfig.pin_vsync = VSYNC_GPIO_NUM;
  cameraConfig.pin_href  = HREF_GPIO_NUM;
  cameraConfig.pin_sccb_sda = SIOD_GPIO_NUM;
  cameraConfig.pin_sccb_scl = SIOC_GPIO_NUM;
  cameraConfig.pin_pwdn  = PWDN_GPIO_NUM;
  cameraConfig.pin_reset = RESET_GPIO_NUM;
  cameraConfig.xclk_freq_hz = CAM_XCLK_HZ;
  cameraConfig.pixel_format = PIXFORMAT_JPEG;
  cameraConfig.frame_size   = CAM_FRAME_SIZE;
  cameraConfig.jpeg_quality = CAM_JPEG_QUALITY;

  cameraConfig.fb_count = 1;
  cameraConfig.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  cameraConfig.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
}

// budzi kamerę (wywoływać z zablokowanym cameraMutex); zwraca false przy błędzie
// sterownik przy inicjalizacji sam ustawia PWDN=0 i konfiguruje czujnik od nowa
static bool wakeCamera() {
  if (esp_camera_init(&cameraConfig) != ESP_OK) return false;
  cameraAwake = true;

  // pierwsze klatki po włączeniu są źle naświetlone (automatyczna ekspozycja i balans bieli jeszcze się ustalają)
  for (uint8_t i = 0; i < CAM_WARMUP_FRAMES; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
  }
  return true;
}

// usypia kamerę (wywoływać z zablokowanym cameraMutex, gdy framesInUse == 0)
// esp_camera_deinit() wyłącza zegar XCLK i DMA, a PWDN=1 przełącza czujnik w tryb czuwania
static void sleepCamera() {
  esp_camera_deinit();
  pinMode(PWDN_GPIO_NUM, OUTPUT);
  digitalWrite(PWDN_GPIO_NUM, HIGH);
  cameraAwake = false;
}

bool cameraInit() {
  esp_log_level_set("cam_hal", ESP_LOG_ERROR);

  cameraMutex = xSemaphoreCreateMutex();
  if (!cameraMutex) return false;

  buildCameraConfig();

  // próbna inicjalizacja - brak kamery wykrywany od razu, a nie przy pierwszym zdjęciu
  if (esp_camera_init(&cameraConfig) != ESP_OK) return false;
  cameraAwake = true;
  sleepCamera();
  return true;
}

void cameraUpdate() {
  if (!cameraMutex) return;
  xSemaphoreTake(cameraMutex, portMAX_DELAY);
  if (cameraAwake && framesInUse == 0 && millis() - lastFrameMs >= CAM_IDLE_TIMEOUT_MS) sleepCamera();
  xSemaphoreGive(cameraMutex);
}

camera_fb_t* cameraGetFrame() {
  // mutex nie jest trzymany podczas esp_camera_fb_get(), bo przy fb_count = 1 pobranie czeka,
  // aż inny użytkownik odda klatkę, a oddanie (cameraReturnFrame) potrzebuje mutexu
  xSemaphoreTake(cameraMutex, portMAX_DELAY);
  if (!cameraAwake && !wakeCamera()) {
    xSemaphoreGive(cameraMutex);
    return NULL;
  }
  framesInUse++;
  xSemaphoreGive(cameraMutex);

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    xSemaphoreTake(cameraMutex, portMAX_DELAY);
    framesInUse--;
    lastFrameMs = millis();
    xSemaphoreGive(cameraMutex);
  }
  return fb;
}

void cameraReturnFrame(camera_fb_t* fb) {
  esp_camera_fb_return(fb);
  xSemaphoreTake(cameraMutex, portMAX_DELAY);
  framesInUse--;
  lastFrameMs = millis();
  xSemaphoreGive(cameraMutex);
}

// wywoływane przez serwer przy zamykaniu każdego połączenia (gniazda używane poza handlerem muszą o tym wiedzieć)
static void onSocketClose(httpd_handle_t hd, int fd) {
  streamOnSocketClose(fd);
  close(fd);
}

bool cameraServerStart() {
  // ignorowane błędy (np. zamknięcie połączenia przez przeglądarkę)
  esp_log_level_set("httpd_txrx", ESP_LOG_NONE);
  esp_log_level_set("httpd_uri", ESP_LOG_NONE);

  httpd_config_t serverConfig = HTTPD_DEFAULT_CONFIG();
  serverConfig.server_port = CAM_SERVER_PORT;
  serverConfig.close_fn = onSocketClose;
  // przy braku wolnych gniazd (max_open_sockets) zamykane jest najdawniej używane połączenie,
  // zamiast odrzucać nowe - inaczej otwarta transmisja i bezczynne połączenia przeglądarki
  // mogłyby zablokować dostęp do /stream/stop
  serverConfig.lru_purge_enable = true;
  if (serverConfig.max_uri_handlers < CAMERA_ENDPOINT_COUNT) serverConfig.max_uri_handlers = CAMERA_ENDPOINT_COUNT;

  // zadanie transmisji musi istnieć przed rejestracją jej endpointów
  if (!streamInit()) return false;
  if (httpd_start(&httpServer, &serverConfig) != ESP_OK) return false;

  bool ok = true;
  for (size_t i = 0; i < CAMERA_ENDPOINT_COUNT; i++) {
    httpd_uri_t uri = {}; // pozostałe pola (np. obsługa WebSocketa) wyzerowane
    uri.uri = CAMERA_ENDPOINTS[i].uri;
    uri.method = CAMERA_ENDPOINTS[i].method;
    uri.handler = CAMERA_ENDPOINTS[i].handler;

    if (httpd_register_uri_handler(httpServer, &uri) != ESP_OK) ok = false; // pozostałe endpointy nadal działają
  }
  return ok;
}

void cameraSetCommonHeaders(httpd_req_t* req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}