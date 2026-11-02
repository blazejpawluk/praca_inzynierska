#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// WIFI
// =====================================================

// const char* ssid = "aaa";
// const char* password = "Blazej_16";

const char* ssid = "Lesna25-strych";
const char* password = "tomeklesna25";

// =====================================================
// LCD I2C
// =====================================================

// Typowy adres LCD I2C: 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Piny I2C ESP32-CAM
#define LCD_SDA 13
#define LCD_SCL 14

// =====================================================
// AI THINKER ESP32-CAM
// OV2640
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// =====================================================
// STREAM
// =====================================================

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;

static const char* STREAM_BOUNDARY =
  "\r\n--" PART_BOUNDARY "\r\n";

static const char* STREAM_PART =
  "Content-Type: image/jpeg\r\n"
  "Content-Length: %u\r\n"
  "\r\n";

// =====================================================
// STRONA WWW
// =====================================================

static esp_err_t index_handler(httpd_req_t *req)
{
  const char* html = R"rawliteral(
<!DOCTYPE html>

<html lang="pl">

<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>ESP32-CAM</title>

<style>

body {
  margin: 0;
  padding: 20px;

  background: #111;
  color: white;

  font-family: Arial, sans-serif;

  text-align: center;
}

h1 {
  margin-bottom: 20px;
}

.camera {
  width: 100%;
  max-width: 800px;

  border: 2px solid #444;

  display: block;

  margin-left: auto;
  margin-right: auto;
}

.info {
  margin-top: 15px;
  color: #aaa;
}

</style>

</head>

<body>

<h1>ESP32-CAM</h1>

<img
  class="camera"
  src="/stream"
  alt="Obraz z kamery"
>

<div class="info">
  Obraz na żywo
</div>

</body>

</html>
)rawliteral";

  httpd_resp_set_type(req, "text/html");

  return httpd_resp_send(
    req,
    html,
    strlen(html)
  );
}

// =====================================================
// STREAMING KAMERY
// =====================================================

static esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;

  esp_err_t res = ESP_OK;

  char part_buf[64];

  // Ustawienie typu odpowiedzi
  res = httpd_resp_set_type(
    req,
    STREAM_CONTENT_TYPE
  );

  if (res != ESP_OK)
  {
    return res;
  }

  // Wyłączenie buforowania
  httpd_resp_set_hdr(
    req,
    "Access-Control-Allow-Origin",
    "*"
  );

  while (true)
  {
    // Pobranie klatki z kamery
    fb = esp_camera_fb_get();

    if (!fb)
    {
      Serial.println(
        "Blad pobierania klatki z kamery"
      );

      res = ESP_FAIL;

      break;
    }

    // Początek kolejnej klatki
    res = httpd_resp_send_chunk(
      req,
      STREAM_BOUNDARY,
      strlen(STREAM_BOUNDARY)
    );

    if (res != ESP_OK)
    {
      esp_camera_fb_return(fb);

      break;
    }

    // Nagłówek JPEG
    size_t hlen = snprintf(
      part_buf,
      sizeof(part_buf),
      STREAM_PART,
      fb->len
    );

    res = httpd_resp_send_chunk(
      req,
      part_buf,
      hlen
    );

    if (res != ESP_OK)
    {
      esp_camera_fb_return(fb);

      break;
    }

    // Dane JPEG
    res = httpd_resp_send_chunk(
      req,
      (const char *)fb->buf,
      fb->len
    );

    // Zwolnienie bufora
    esp_camera_fb_return(fb);

    if (res != ESP_OK)
    {
      break;
    }

    // Małe opóźnienie stabilizujące streaming
    delay(30);
  }

  return res;
}

// =====================================================
// URUCHOMIENIE SERWERA
// =====================================================

void startCameraServer()
{
  httpd_config_t config =
    HTTPD_DEFAULT_CONFIG();

  config.server_port = 80;

  httpd_handle_t server = NULL;

  // ---------------------------------------------------
  // STRONA GŁÓWNA
  // ---------------------------------------------------

  httpd_uri_t index_uri;

  index_uri.uri = "/";
  index_uri.method = HTTP_GET;
  index_uri.handler = index_handler;
  index_uri.user_ctx = NULL;

  // ---------------------------------------------------
  // STREAM
  // ---------------------------------------------------

  httpd_uri_t stream_uri;

  stream_uri.uri = "/stream";
  stream_uri.method = HTTP_GET;
  stream_uri.handler = stream_handler;
  stream_uri.user_ctx = NULL;

  // ---------------------------------------------------
  // START SERWERA
  // ---------------------------------------------------

  if (httpd_start(
        &server,
        &config
      ) == ESP_OK)
  {
    httpd_register_uri_handler(
      server,
      &index_uri
    );

    httpd_register_uri_handler(
      server,
      &stream_uri
    );

    Serial.println(
      "Serwer HTTP uruchomiony."
    );
  }
  else
  {
    Serial.println(
      "BLAD: nie mozna uruchomic serwera HTTP."
    );
  }
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);

  // ===================================================
  // LCD
  // ===================================================

  // Uruchomienie magistrali I2C
  Wire.begin(
    LCD_SDA,
    LCD_SCL
  );

  // Uruchomienie LCD
  lcd.init();

  lcd.backlight();

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Uruchamianie...");

  // ===================================================
  // SERIAL
  // ===================================================

  Serial.println();
  Serial.println("==============================");
  Serial.println("ESP32-CAM");
  Serial.println("TEST STREAMINGU");
  Serial.println("==============================");

  // ===================================================
  // KONFIGURACJA KAMERY
  // ===================================================

  camera_config_t config;

  config.ledc_channel =
    LEDC_CHANNEL_0;

  config.ledc_timer =
    LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk =
    XCLK_GPIO_NUM;

  config.pin_pclk =
    PCLK_GPIO_NUM;

  config.pin_vsync =
    VSYNC_GPIO_NUM;

  config.pin_href =
    HREF_GPIO_NUM;

  config.pin_sccb_sda =
    SIOD_GPIO_NUM;

  config.pin_sccb_scl =
    SIOC_GPIO_NUM;

  config.pin_pwdn =
    PWDN_GPIO_NUM;

  config.pin_reset =
    RESET_GPIO_NUM;

  config.xclk_freq_hz =
    20000000;

  config.pixel_format =
    PIXFORMAT_JPEG;

  // ===================================================
  // PARAMETRY OBRAZU
  // ===================================================

  if (psramFound())
  {
    Serial.println(
      "PSRAM: OK"
    );

    config.frame_size =
      FRAMESIZE_VGA;

    config.jpeg_quality =
      12;

    config.fb_count =
      2;
  }
  else
  {
    Serial.println(
      "PSRAM: BRAK"
    );

    config.frame_size =
      FRAMESIZE_QVGA;

    config.jpeg_quality =
      15;

    config.fb_count =
      1;
  }

  // ===================================================
  // URUCHOMIENIE KAMERY
  // ===================================================

  Serial.println(
    "Uruchamianie kamery..."
  );

  esp_err_t err =
    esp_camera_init(&config);

  if (err != ESP_OK)
  {
    Serial.printf(
      "BLAD KAMERY: 0x%x\n",
      err
    );

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BLAD KAMERY");

    lcd.setCursor(0, 1);

    char errorText[17];

    snprintf(
      errorText,
      sizeof(errorText),
      "Kod: 0x%x",
      err
    );

    lcd.print(errorText);

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "Kamera uruchomiona."
  );

  // ===================================================
  // WIFI
  // ===================================================

  WiFi.mode(WIFI_STA);

  Serial.println();
  Serial.println(
    "Laczenie z Wi-Fi..."
  );

  WiFi.begin(
    ssid,
    password
  );

  int attempts = 0;

  while (
    WiFi.status() != WL_CONNECTED &&
    attempts < 40
  )
  {
    delay(500);

    Serial.print(".");

    attempts++;
  }

  Serial.println();

  // ===================================================
  // SPRAWDZENIE WIFI
  // ===================================================

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println(
      "BLAD: brak polaczenia z Wi-Fi."
    );

    Serial.println(
      "Sprawdz SSID, haslo i siec 2.4 GHz."
    );

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Blad WiFi");

    lcd.setCursor(0, 1);
    lcd.print("Brak polaczenia");

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println(
    "Wi-Fi polaczone."
  );

  Serial.print(
    "Adres IP: "
  );

  Serial.println(
    WiFi.localIP()
  );

  Serial.print(
    "Sila sygnalu RSSI: "
  );

  Serial.print(
    WiFi.RSSI()
  );

  Serial.println(
    " dBm"
  );

  // ===================================================
  // START SERWERA
  // ===================================================

  startCameraServer();

  // ===================================================
  // LCD - GOTOWE
  // ===================================================

  lcd.clear();

  // Pierwsza linia - nazwa WiFi
  lcd.setCursor(0, 0);
  lcd.print(ssid);

  // Druga linia - adres IP
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  // ===================================================
  // INFORMACJE SERIAL
  // ===================================================

  Serial.println();
  Serial.println("==============================");
  Serial.println("STREAMING GOTOWY");
  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("==============================");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
  delay(1000);
}