// Błażej Pawluk
// plik komunikacji przez UART z ESP32-CAM, a następnie do strony internetowej z interfejsem użytkownika
// zbieranie realnych danych z sensorów i wykonywanie otrzymanych poleceń (z silnikami)
// wyliczanie pozycji i kąta przy 100 Hz: droga z enkoderów, kąt z żyroskopu, filtr Kalmana prędkości (pozycja.cpp)

// połączenie z ESP32-CAM (przez 2-kierunkowy konwerter poziomów):
// D0 (RX) - IO14
// D1 (TX) - IO13

// podłączone komponenty:
// enkodery:  A0 (lewy), A1 (prawy)
// sonar:     A3 (TRIG), A2 (ECHO)
// serwo:     D9
// silniki (mostek H L298N): D6 (ENB), D7 (IN3), D8 (IN4) - lewy, D3 (ENA), D4 (IN1), D5 (IN2) - prawy
// LCD i MPU: I2C (A4 - SDA, A5 - SCL dla LCD, SDA, SCL dla MPU)

// co 0,2s wysyła linię z danymi z sensorów i linię z pozycją
// otrzymane polecenia są wykonywane i wyświetlane na LCD

// wysyłane dane:
//    D:<enL>,<enR>,<sonar>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<mL>,<mR>
//    jednostki: enkodery: liczba impulsów, sonar: cm (0 - brak echa / brak pomiaru), akcelerometr: g, żyroskop: stopnie/s,
//               silniki: aktualna prędkość [-255,255]
//    P:<x>,<y>,<kąt>,<v>,<poślizg>,<postój>
//    jednostki: x, y: cm (X - przód, Y - lewo względem pozycji startowej), kąt: stopnie (-180,180], skręt w lewo dodatni,
//               v: cm/s (ujemna - do tyłu), poślizg, postój: 0/1

// odbierane dane:
//    C:S,<angle> - kąt serwa [45,135]
//    C:TRIG      - wysłanie sygnału TRIG z sonaru (wynik w kolejnych ramkach danych)
//    C:M,<l>,<r> - prędkości lewego i prawego koła [-255,255] (ujemne - do tyłu)
//    C:STOP      - zatrzymanie silników
//    C:RESETPOS  - wyzerowanie pozycji i kąta
//    brak polecenia C:M przez 1s zatrzymuje silniki

#include "lcd.h"
#include "komunikacja.h"
#include "enkodery.h"
#include "sonar.h"
#include "serwo.h"
#include "zyroskop.h"
#include "silniki.h"
#include "pozycja.h"

// konfiguracja UART
const unsigned long BAUD = 38400;
const unsigned long SEND_INTERVAL = 200; // 200ms

// czas ostatniego wysłanego zestawu danych
unsigned long lastSend = 0;
void setup() {
  initMotors();
  initServo();
  initLCD();
  initEncoders();
  initSonar();

  if (initMPU()) {
    showStatus("Kalibracja...", "NIE RUSZAJ!");
    calibrateGyro();
  } else {
    showStatus("Blad MPU", "");
    delay(2000);
  }

  initPosition();

  // Serial dopiero po kalibracji, żeby nie zbierać poleceń w buforze
  Serial.begin(BAUD);

  showStatus("AUTKO gotowe", "czekam na dane");

  lastSend = millis();
}

void loop() {
  unsigned long now = millis();
  readSerial();
  if (updateMPU(now)) updatePosition();
  updateMotors(now);

  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    sendData();
  }
}
