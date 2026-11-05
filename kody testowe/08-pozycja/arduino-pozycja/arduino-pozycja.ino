// Błażej Pawluk
// plik komunikacji przez UART z ESP32-CAM, a następnie do strony internetowej z interfejsem użytkownika
// zbieranie realnych danych z sensorów i wykonywanie otrzymanych poleceń (z silnikami)

// połączenie z ESP32-CAM (przez 2-kierunkowy konwerter poziomów):
// D0 (RX) - IO14
// D1 (TX) - IO13

// podłączone komponenty:
// enkodery:  A0 (lewy), A1 (prawy)
// sonar:     A3 (TRIG), A2 (ECHO)
// serwo:     D9
// silniki (mostek H L298N): D3 (ENA), D4 (IN1), D5 (IN2) - lewy, D6 (ENB), D7 (IN3), D8 (IN4) - prawy
// LCD i MPU: I2C (A4 - SDA, A5 - SCL dla LCD, SDA, SCL dla MPU)

// co 0,2s wysyła linię z danymi z sensorów
// otrzymane polecenia są wykonywane i wyświetlane na LCD

// wysyłane dane:
//    D:<enL>,<enR>,<sonar>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>,<mL>,<mR>
//    jednostki: enkodery: liczba impulsów, sonar: cm (0 - brak echa / brak pomiaru), akcelerometr: g, żyroskop: stopnie/s,
//               silniki: aktualna prędkość [-255,255]

// odbierane dane:
//    C:S,<angle> - kąt serwa [45,135]
//    C:TRIG      - wysłanie sygnału TRIG z sonaru (wynik w kolejnych ramkach danych)
//    C:M,<l>,<r> - prędkości lewego i prawego koła [-255,255] (ujemne - do tyłu)
//    C:STOP      - zatrzymanie silników
//    brak polecenia C:M przez 1s zatrzymuje silniki

#include "lcd.h"
#include "komunikacja.h"
#include "enkodery.h"
#include "sonar.h"
#include "serwo.h"
#include "zyroskop.h"
#include "silniki.h"

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

  // Serial dopiero po kalibracji, żeby nie zbierać poleceń w buforze
  Serial.begin(BAUD);

  showStatus("AUTKO gotowe", "czekam na dane");

  lastSend = millis();
}

void loop() {
  unsigned long now = millis();
  readSerial();
  updateMPU(now);
  updateMotors(now);

  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    sendData();
  }
}
