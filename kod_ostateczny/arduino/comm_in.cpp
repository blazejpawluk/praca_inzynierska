/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja komunikacji przychodzącej (opis i protokół w comm_in.h)
*/

#include "comm_in.h"
#include "lcd.h"
#include "wheels.h"
#include "servo.h"
#include "radar.h"
#include "position.h"

ConnectionStatus connectionStatus = CONN_WAIT_AP;

// bufor odbiorczy; najdłuższa poprawna komenda: "W:RADAR:-32767,-32767,-32767,-32767" (35 znaków) + '\0';
// przy rozszerzaniu protokołu trzeba sprawdzić, czy nowe komendy się mieszczą
const uint8_t RX_BUF_SIZE = 40;
static char rxBuf[RX_BUF_SIZE];
static uint8_t rxLen = 0;

// timeout komend
const unsigned long CMD_TIMEOUT_MS = 1000;
static unsigned long lastCmdMs = 0;

// ostatnia komenda W: pokazywana w wierszu LCD_ROW_CMD najwyżej co LCD_REFRESH_MS
// (zapis jednego znaku przez I2C trwa ~1.4 ms, więc odświeżanie przy każdej komendzie blokowałoby pętlę)
const unsigned long LCD_REFRESH_MS = 250;
static char lastCmd[LCD_COLS + 1];
static bool lastCmdPending = false;
static unsigned long lastLcdMs = 0;

const uint8_t MAX_ARGS = 4;

// parsuje dokładnie count liczb całkowitych oddzielonych przecinkami (np. "-120,35") do out;
// zwraca false, jeśli pojawi się znak inny niż 0-9, '-' (na początku liczby) i ',',
// liczba wartości się nie zgadza lub wartość wykracza poza zakres int16_t
static bool parseArgs(const char* s, long* out, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    bool neg = false;
    if (*s == '-') {
      neg = true;
      s++;
    }
    if (*s < '0' || *s > '9') return false;

    long v = 0;
    while (*s >= '0' && *s <= '9') {
      v = v * 10 + (*s++ - '0');
      if (v > INT16_MAX) return false;
    }

    out[i] = neg ? -v : v;

    if (i < count - 1) {
      if (*s != ',') return false;
      s++;
    }
  }
  return *s == '\0';
}

// sprawdza, czy linia zaczyna się od prefix (napis w pamięci programu);
// jeśli tak, ustawia args na tekst po prefiksie
static bool startsWith(const char* line, PGM_P prefix, const char*& args) {
  size_t len = strlen_P(prefix);
  if (strncmp_P(line, prefix, len) != 0) return false;
  args = line + len;
  return true;
}

static void handleLine(const char* line) {
  if (line[0] == 'W' && connectionStatus != CONN_OK) return;

  long args[MAX_ARGS];
  const char* rest;

  // W:SERVO:<angle>
  if (startsWith(line, PSTR("W:SERVO:"), rest)) {
    if (!parseArgs(rest, args, 1)) return;
    servoSetAngle(args[0]);
  
  // W:MOTOR:<left>,<right>
  } else if (startsWith(line, PSTR("W:MOTOR:"), rest)) {
    if (!parseArgs(rest, args, 2)) return;
    motorsSet(args[0], args[1]);

  // W:RESETPOS
  } else if (strcmp_P(line, PSTR("W:RESETPOS")) == 0) {
    positionReset();

  // W:RADAR:<from>,<to>,<step>,<interval>
  } else if (startsWith(line, PSTR("W:RADAR:"), rest)) {
    if (!parseArgs(rest, args, 4)) return;
    radarStart(args[0], args[1], args[2], args[3]);

  // W:RADAROFF
  } else if (strcmp_P(line, PSTR("W:RADAROFF")) == 0) {
    radarStop();

  // W:MSG:<text>
  } else if (startsWith(line, PSTR("W:MSG:"), rest)) {
    lcdPrint(LCD_ROW_STATUS, rest);
    lastCmdMs = millis();
    return;

  // E:CONNECTION:<status>
  } else if (startsWith(line, PSTR("E:CONNECTION:"), rest)) {
    if (!parseArgs(rest, args, 1)) return;
    if (args[0] < CONN_WAIT_AP || args[0] > CONN_OK) return; // nieznany status
    connectionStatus = (ConnectionStatus)args[0];
    return;

  // [unknown command]
  } else {
    return; // nieznana komenda
  }

  lastCmdMs = millis();

  // zapamiętanie komendy do wyświetlenia przez commShowLastCommand()
  strncpy(lastCmd, line, LCD_COLS);
  lastCmd[LCD_COLS] = '\0';
  lastCmdPending = true;
}

void commReceive() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rxBuf[rxLen] = '\0';
      if (rxLen > 0) handleLine(rxBuf);
      rxLen = 0;
    } else if (rxLen < RX_BUF_SIZE - 1) {
      rxBuf[rxLen++] = c; // znaki ponad rozmiar bufora są pomijane
    }
  }
}

void commShowLastCommand() {
  if (!lastCmdPending || millis() - lastLcdMs < LCD_REFRESH_MS) return;
  lcdPrint(LCD_ROW_CMD, lastCmd);
  lastCmdPending = false;
  lastLcdMs = millis();
}

void commClearLastCommand() {
  lastCmdPending = false;
}

void commCheckTimeout() {
  if ((motorSpeedL != 0 || motorSpeedR != 0) && millis() - lastCmdMs > CMD_TIMEOUT_MS) {
    motorsSet(0, 0);
    lcdPrint(LCD_ROW_CMD, F("NO COMMANDS"));
  }
}