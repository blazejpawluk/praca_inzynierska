/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca komunikację przychodzącą przez UART z ESP32-CAM

  Połączenie: UART RX=D0 (z ESP32-CAM przez konwerter poziomów logicznych z 3.3V na 5V)
  
  Protokoły komunikacji przychodzącej na Arduino:
    - W:SERVO:<angle> - ustawienie kąta SG90 [stopnie]
    - W:MOTOR:<left>,<right> - ustwienie prędkości silników [PWM]
    - W:RESETPOS - reset pozycji do (0,0,0)
    - W:RADAR:<from>,<to>,<step>,<interval> - uruchomianie radaru [stopnie, stopnie, stopnie, ms]
    - W:RADAROFF - wyłączenie radaru
    - W:MSG:<text> - tekst do wyświetlenia na LCD (maksymalnie 16 znaków)
    - E:CONNECTION:<status> - stataus połączenia z komputerem [0=oczekiwanie na uruchomienie AccessPoint, 1=oczekiwanie na połączenie z komputerem, 2=połączono]
*/

#pragma once

#include <Arduino.h>
#include "lcd.h"
#include "wheels.h"
#include "servo.h"
#include "radar.h"

// konfiguracja
const uint8_t RX_BUF_SIZE = 128;
char rxBuf[RX_BUF_SIZE];
uint8_t rxLen;
extern uint8_t connectionStatus;

bool parseArgs(const char* s, long* out, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    bool neg = false;
    if (*s == '-') {
      neg = true;
      s++;
    }
    if (*s < '0' || *s > '9') return false;
    
    long v = 0;
    uint8_t digits = 0;
    while (*s >= '0' && *s <= '9') {
      v = v * 10 + (*s++ - '0');
    }

    out[i] = neg ? -v : v;
    
    if (i < count - 1) {
      if (*s != ',') return false;
      s++;
    }
  }
  return *s == '\0';
}

void handleLine(const char* line) {
  long p[4];

  if (strncmp(line, "W:SERVO:", 8) == 0) {
    if (!parseArgs(line + 8, p, 1)) return;
    setServoAngle(p[0]);
  } else if (strncmp(line, "W:MOTOR:", 8) == 0) {
    if (!parseArgs(line + 8, p, 2)) return;
    setMotors(p[0], p[1]);
  } else if (strcmp(line, "W:RESETPOS") == 0) {
    resetPosition();
  } else if (strncmp(line, "W:RADAR:", 8) == 0) {
    if (!parseArgs(line + 8, p, 4)) return;
    startRadar(p[0], p[1], p[2], p[3]);
  } else if (strcmp(line, "W:RADAROFF") == 0) {
    stopRadar();
  } else if (strncmp(line, "W:MSG:", 6) == 0) {
    printLCD(0, line + 6);
  } else if (strncmp(line, "E:CONNECTION:", 13) == 0) {
    if (!parseArgs(line + 13, p, 1)) return;
    connectionStatus = p[0];
  } else return;

  // tylko poprawne komendy trafiają na LCD
  printLCD(1, line);
}

void readSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      if (rxLen > 0) handleLine(rxBuf);
      rxLen = 0;
    } else if (rxLen < RX_BUF_SIZE - 1) {
      rxBuf[rxLen++] = c;
    }
  }
}