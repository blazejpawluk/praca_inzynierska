/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Kod na mikrokontroler Arduino UNO

  Podłączone komponenty:
    - 4 silniki DC z przekładnią sterowane przez sterownik 2-osiowy L298N
      piny sterujące: ENA=5, IN1=4, IN2=3, ENB=6, IN3=7, IN4=8
    - ESP32-CAM
      połączenie UART: RX=D0, TX=D1 (przez konwerter poziomów logicznych 5V-3.3V)
    - wyświetlacz LCD 16x2 I2C
      połączenie I2C: SDA=A4, SCL=A5
    - enkodery szczelinowe kół (po jednym na przednie koło)
      piny sterujące: lewy=A0, prawy=A1
    - serwomechanizm SG90
      pin sterujący: D9
    - sonar ultradźwiękowy HC-SR04
      piny sterujące: TRIG=A3, ECHO=A2
    - żyroskop z akcelerometrem MPU6050
      połączenie I2C: SDA=A4, SCL=A5 (wspólne z LCD)
*/

#include "i2c.h"
#include "comm_in.h"
#include "comm_out.h"
#include "lcd.h"
#include "wheels.h"
#include "encoders.h"
#include "servo.h"
#include "sonar.h"
#include "interrupts.h"
#include "mpu.h"
#include "position.h"
#include "radar.h"

// konfiguracja systemu
const unsigned long BAUD = 38400;
const unsigned long SEND_INTERVAL_MS = 100; // czas między kolejnymi cyklami wysyłania danych
const unsigned long I2C_RETRY_MS = 1000; // odstęp między próbami odzyskania magistrali I2C

static unsigned long lastSendMs = 0;
static unsigned long lastI2cRetryMs = 0;
static unsigned int i2cRecoveries = 0;
static ConnectionStatus prevStatus = CONN_WAIT_AP;

// po awarii I2C zatrzymuje pojazd (bez żyroskopu pozycja jest nieznana) i próbuje odzyskać magistralę
static void handleI2cFault() {
  if (i2cReady()) return;

  motorsSet(0, 0);
  if (millis() - lastI2cRetryMs < I2C_RETRY_MS) return;
  lastI2cRetryMs = millis();

  if (!i2cRecover() || !mpuConfigure()) {
    i2cMarkFault();
    return;
  }

  lcdRecover();
  positionInit();
  commFlushInput(); // bufor UART mógł się przepełnić podczas odzyskiwania

  // licznik zostaje na LCD, aby było widać, że awaria wystąpiła
  char text[LCD_COLS + 1];
  snprintf_P(text, sizeof(text), PSTR("I2C RESETS: %u"), ++i2cRecoveries);
  lcdPrint(LCD_ROW_STATUS, text);
}

// inicjalizuje MPU6050, ponawiając próby aż do skutku
static void initMpu() {
  lcdPrint(LCD_ROW_CMD, F("GYRO CALIBRATION"));
  while (!mpuInit()) {
    lcdPrint(LCD_ROW_STATUS, F("MPU6050 ERROR"));
    lcdPrint(LCD_ROW_CMD, F("RETRYING"));
    delay(I2C_RETRY_MS);

    if (i2cRecover()) lcdRecover();
    lcdPrint(LCD_ROW_STATUS, F("INITIALIZATION"));
    lcdPrint(LCD_ROW_CMD, F("GYRO CALIBRATION"));
  }
}

// czeka, dopóki status połączenia jest równy status, wyświetlając komunikat
static void waitWhileStatus(ConnectionStatus status, const __FlashStringHelper* line0, const __FlashStringHelper* line1) {
  while (connectionStatus == status) {
    commRequestConnection();
    commReceive();
    handleI2cFault();

    lcdPrint(LCD_ROW_STATUS, line0);
    lcdPrint(LCD_ROW_CMD, line1);
  }
}

// przejście do stanu połączenia (start oraz ponowne połączenie)
static void onConnected() {
  positionInit();
  lcdPrint(LCD_ROW_STATUS, F("CONNECTED"));
  lcdPrint(LCD_ROW_CMD, F("READY TO WORK"));
}

// przejście do stanu braku połączenia
static void onDisconnected() {
  motorsSet(0, 0);

  commClearLastCommand();
  lcdPrint(LCD_ROW_STATUS, F("DISCONNECTED"));
  lcdPrint(LCD_ROW_CMD, F("WAITING"));

  // porzucenie trwającego pomiaru sonaru, aby radar nie czekał na niego po ponownym połączeniu
  sonarCancel();
  radarPause();
  commResetSendCycle();
}

// przekazuje wynik zakończonego pomiaru sonaru do wysłania i do radaru
static void handleSonarResult() {
  unsigned int mm;
  int angle;
  Pose pose;
  if (!sonarPoll(mm, angle, pose)) return;

  commQueueDist(angle, mm, pose);
  radarOnMeasurement();
}

// praca przy aktywnym połączeniu
static void runConnected() {
  commCheckTimeout();
  positionPoll();
  radarUpdate();
  handleSonarResult();

  unsigned long now = millis();
  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    commStartSendCycle();
    lastSendMs = now;
  }
  commSendNext();
}

// praca bez połączenia
static void runDisconnected() {
  commRequestConnection();
}

void setup() {
  // magistrala I2C wspólna dla LCD i MPU6050
  i2cInit();

  lcdInit();
  lcdPrint(LCD_ROW_STATUS, F("INITIALIZATION"));

  Serial.begin(BAUD);

  motorsInit();
  encodersInit();
  servoInit();
  sonarInit();
  interruptsInit();

  // mpuInit() kalibruje też żyroskop - pojazd nie może się wtedy ruszać
  initMpu();

  // oczekiwanie na połączenie
  waitWhileStatus(CONN_WAIT_AP, F("WAITING FOR WIFI"), F(""));
  waitWhileStatus(CONN_WAIT_CLIENT, F("CONNECT ON YOUR"), F("DEVICE"));

  onConnected();
  prevStatus = CONN_OK;
  lastSendMs = millis();
}

void loop() {
  commReceive();
  handleI2cFault();
  commShowLastCommand();

  ConnectionStatus status = connectionStatus;
  if (status == CONN_OK && prevStatus != CONN_OK) onConnected();
  if (status != CONN_OK && prevStatus == CONN_OK) onDisconnected();
  prevStatus = status;

  if (status == CONN_OK) runConnected();
  else runDisconnected();
}