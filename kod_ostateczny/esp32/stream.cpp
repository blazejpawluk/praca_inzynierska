/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja transmisji na żywo z kamery (opis w stream.h)
*/

#include <Arduino.h>
#include "camera.h"
#include "stream.h"
#include "lwip/sockets.h"

// konfiguracja transmisji
const unsigned long STREAM_FRAME_INTERVAL_MS = 100; // minimalny czas między klatkami (max 10 klatek/s)
const unsigned long STREAM_IDLE_DELAY_MS = 50; // czas oczekiwania, gdy nie ma czego wysyłać
const uint32_t STREAM_TASK_STACK = 4096;
const UBaseType_t STREAM_TASK_PRIORITY = 1;

#define STREAM_BOUNDARY "frame"

static const char STREAM_RESPONSE_HEADER[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY "\r\n"
  "Access-Control-Allow-Origin: *\r\n"
  "Cache-Control: no-store\r\n"
  "Connection: close\r\n"
  "\r\n";

// nagłówek każdej klatki; %u zastępowane długością JPEG
static const char STREAM_PART_HEADER[] =
  "--" STREAM_BOUNDARY "\r\n"
  "Content-Type: image/jpeg\r\n"
  "Content-Length: %u\r\n"
  "\r\n";

const size_t UINT32_MAX_DIGITS = 10; // miejsce na długość klatki w nagłówku

// stan transmisji; chroniony przez streamMutex, który zadanie trzyma także podczas wysyłania klatki,
// dzięki czemu serwer nie zamknie gniazda (streamOnSocketClose) w trakcie wysyłania;
// wysyłanie do zawieszonego odbiorcy może trzymać streamMutex do send_wait_timeout serwera (domyślnie 5 s)
static SemaphoreHandle_t streamMutex = NULL;
static bool enabled = false;
static httpd_handle_t clientServer = NULL;
static int clientFd = -1;

// wysyła cały bufor (httpd_socket_send może wysłać tylko część); zwraca false przy błędzie
static bool sendAll(httpd_handle_t hd, int fd, const char* buf, size_t len) {
  while (len > 0) {
    int sent = httpd_socket_send(hd, fd, buf, len, 0);
    if (sent <= 0) return false;
    buf += sent;
    len -= sent;
  }
  return true;
}

// rozłącza odbiorcę (wywoływać z zablokowanym mutexem)
static void disconnectClient() {
  if (clientFd < 0) return;
  shutdown(clientFd, SHUT_RDWR);
  clientFd = -1;
  clientServer = NULL;
}

static bool sendFrame(httpd_handle_t hd, int fd, const camera_fb_t* fb) {
  char partHeader[sizeof(STREAM_PART_HEADER) + UINT32_MAX_DIGITS];
  int headerLen = snprintf(partHeader, sizeof(partHeader), STREAM_PART_HEADER, (unsigned)fb->len);

  return sendAll(hd, fd, partHeader, headerLen)
      && sendAll(hd, fd, (const char*)fb->buf, fb->len)
      && sendAll(hd, fd, "\r\n", 2);
}

static bool isStreaming() {
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  bool active = enabled && clientFd >= 0;
  xSemaphoreGive(streamMutex);
  return active;
}

static void streamTask(void*) {
  while (true) {
    unsigned long frameStartMs = millis();
    bool active = isStreaming();

    if (active) {
      // klatka jest pobierana bez mutexu, bo cameraGetFrame() może czekać (budzenie kamery, /capture trzyma bufor)
      camera_fb_t* fb = cameraGetFrame();
      if (fb) { // chwilowy brak klatki nie przerywa transmisji
        // stan mógł się zmienić podczas pobierania klatki
        xSemaphoreTake(streamMutex, portMAX_DELAY);
        if (enabled && clientFd >= 0 && !sendFrame(clientServer, clientFd, fb)) disconnectClient();
        xSemaphoreGive(streamMutex);
        cameraReturnFrame(fb);
      }
    }

    unsigned long waitMs = active ? STREAM_FRAME_INTERVAL_MS : STREAM_IDLE_DELAY_MS;
    unsigned long elapsedMs = millis() - frameStartMs;
    vTaskDelay(pdMS_TO_TICKS(elapsedMs < waitMs ? waitMs - elapsedMs : 1));
  }
}

bool streamInit() {
  streamMutex = xSemaphoreCreateMutex();
  if (!streamMutex) return false;
  return xTaskCreate(streamTask, "stream", STREAM_TASK_STACK, NULL, STREAM_TASK_PRIORITY, NULL) == pdPASS;
}

void streamOnSocketClose(int fd) {
  if (!streamMutex) return;
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  if (fd == clientFd) {
    clientFd = -1;
    clientServer = NULL;
  }
  xSemaphoreGive(streamMutex);
}

esp_err_t streamStartHandler(httpd_req_t* req) {
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  enabled = true;
  xSemaphoreGive(streamMutex);

  cameraSetCommonHeaders(req);
  return httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
}

esp_err_t streamHandler(httpd_req_t* req) {
  xSemaphoreTake(streamMutex, portMAX_DELAY);

  if (!enabled) {
    xSemaphoreGive(streamMutex);
    cameraSetCommonHeaders(req);
    httpd_resp_set_status(req, "409 Conflict");
    return httpd_resp_send(req, "transmisja wylaczona", HTTPD_RESP_USE_STRLEN);
  }

  disconnectClient(); // tylko jeden odbiorca

  // nagłówek odpowiedzi jest wysyłany ręcznie, a klatki dosyła streamTask() przez to samo gniazdo
  const int headerLen = sizeof(STREAM_RESPONSE_HEADER) - 1;
  if (httpd_send(req, STREAM_RESPONSE_HEADER, headerLen) != headerLen) {
    xSemaphoreGive(streamMutex);
    return ESP_FAIL;
  }
  clientServer = req->handle;
  clientFd = httpd_req_to_sockfd(req);

  xSemaphoreGive(streamMutex);
  return ESP_OK;
}

esp_err_t streamStopHandler(httpd_req_t* req) {
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  enabled = false;
  disconnectClient();
  xSemaphoreGive(streamMutex);

  cameraSetCommonHeaders(req);
  return httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
}