/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja komunikacji z Arduino UNO (opis i protokół w comm_arduino.h)
*/

#include "comm_arduino.h"
#include "comm_ws.h"

// konfiguracja UART
const int UART_RX_PIN = 13;
const int UART_TX_PIN = 14;
const unsigned long UART_BAUD = 19200;

// bufor sprzętowego UART
const size_t UART_RX_BUFFER_SIZE = 16384;

// bufor odbiorczy linii; najdłuższa linia z Arduino (A:MPU) ma 57 znaków bez "\r\n"
const size_t RX_BUF_SIZE = 256;
static char rxBuf[RX_BUF_SIZE];
static size_t rxLen = 0;
static bool rxOverflow = false;

// ustawiane przez zadanie UART po utracie danych (przepełnienie bufora jest czyszczone przez sterownik)
static volatile bool rxError = false;

// najdłuższa komenda przepuszczana do Arduino (najdłuższa poprawna ma 35 znaków, bufor Arduino mieści 127);
// ograniczenie chroni 64-bajtowy bufor sprzętowy UART Arduino
const size_t CMD_MAX_LEN = 39;

// odstęp między komendami wysyłanymi do Arduino; przy 19200 bodów najdłuższa komenda z '\n' (40 B)
// i E:CONNECTION (16 B) trwają razem ~29 ms
const unsigned long CMD_MIN_INTERVAL_MS = 30;

// kolejka komend czekających na wysłanie (z zapasem - w kolejce jest najwyżej jedna komenda danego rodzaju)
const size_t CMD_QUEUE_SIZE = 32;
static char cmdQueue[CMD_QUEUE_SIZE][CMD_MAX_LEN + 1];
static size_t cmdQueueLen = 0;
static unsigned long lastCmdSentMs = 0;

// status jest powtarzany co STATUS_REPEAT_MS, bo Arduino przy CONN_OK nie wysyła A:CONNECTION
// i bez tego uszkodzona linia E:CONNECTION zostawiłaby je w złym stanie
const unsigned long STATUS_REPEAT_MS = 1000;
static unsigned long lastStatusMs = 0;

static ConnectionStatus connectionStatus = CONN_WAIT_AP;

static void sendConnectionStatus() {
  Serial2.print("E:CONNECTION:");
  Serial2.println((int)connectionStatus);
  lastStatusMs = millis();
}

const uint8_t ASCII_PRINTABLE_FIRST = 0x20; // spacja
const uint8_t ASCII_PRINTABLE_LAST = 0x7E;  // '~'

// sprawdza, czy tekst zawiera tylko drukowalne znaki ASCII
// (WebSocket wymaga poprawnego UTF-8 - przeglądarka zrywa połączenie po otrzymaniu śmieci z UART)
static bool isPrintable(const uint8_t* s, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (s[i] < ASCII_PRINTABLE_FIRST || s[i] > ASCII_PRINTABLE_LAST) return false;
  }
  return true;
}

// długość rodzaju komendy: do drugiego ':' włącznie ("W:MOTOR:100,100" > "W:MOTOR:")
// albo cała komenda, jeśli nie ma argumentów ("W:RESETPOS")
static size_t commandTypeLength(const char* cmd, size_t length) {
  const char* colon = (const char*)memchr(cmd + 2, ':', length - 2);
  return colon ? colon - cmd + 1 : length;
}

static void removeQueuedCommand(size_t index) {
  memmove(cmdQueue[index], cmdQueue[index + 1], (cmdQueueLen - index - 1) * sizeof(cmdQueue[0]));
  cmdQueueLen--;
}

// dodaje komendę na koniec kolejki, usuwając wcześniejszą komendę tego samego rodzaju
// (nowa trafia na koniec, aby zachować kolejność, np. W:RADAR, W:RADAROFF, W:RADAR > W:RADAROFF, W:RADAR)
static void queueCommand(const char* cmd, size_t length) {
  size_t typeLen = commandTypeLength(cmd, length);
  for (size_t i = 0; i < cmdQueueLen; i++) {
    if (commandTypeLength(cmdQueue[i], strlen(cmdQueue[i])) == typeLen && strncmp(cmdQueue[i], cmd, typeLen) == 0) {
      removeQueuedCommand(i);
      break; // w kolejce jest najwyżej jedna komenda danego rodzaju
    }
  }
  if (cmdQueueLen == CMD_QUEUE_SIZE) return; // możliwe tylko przy nieznanych rodzajach komend

  memcpy(cmdQueue[cmdQueueLen], cmd, length);
  cmdQueue[cmdQueueLen][length] = '\0';
  cmdQueueLen++;
}

// wysyła najstarszą komendę z kolejki, jeśli minął CMD_MIN_INTERVAL_MS od poprzedniej
static void sendQueuedCommand() {
  if (cmdQueueLen == 0 || millis() - lastCmdSentMs < CMD_MIN_INTERVAL_MS) return;
  Serial2.print(cmdQueue[0]);
  Serial2.write('\n');
  lastCmdSentMs = millis();
  removeQueuedCommand(0);
}

static void handleLine(const char* line, size_t length) {
  if (strcmp(line, "A:CONNECTION") == 0) {
    sendConnectionStatus();
    return;
  }
  if (length < 2 || line[0] != 'A' || line[1] != ':') return;
  if (!isPrintable((const uint8_t*)line, length)) return;
  wsBroadcast(line);
}

static void onUartError(hardwareSerial_error_t err) {
  if (err == UART_BUFFER_FULL_ERROR || err == UART_FIFO_OVF_ERROR) rxError = true;
}

void arduinoInit() {
  Serial2.setRxBufferSize(UART_RX_BUFFER_SIZE); // musi być przed begin()
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
  Serial2.onReceiveError(onUartError);
  sendConnectionStatus();
}

void arduinoUpdate() {
  if (millis() - lastStatusMs >= STATUS_REPEAT_MS) sendConnectionStatus();
  sendQueuedCommand();

  while (Serial2.available() > 0) {
    // po utracie danych bieżąca linia jest niekompletna - odrzucamy ją do końca
    // (sprawdzane przy każdym bajcie, bo błąd zgłasza inne zadanie w trakcie odczytu)
    if (rxError) {
      rxError = false;
      rxOverflow = true;
    }

    char c = (char)Serial2.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (rxLen > 0 && !rxOverflow) {
        rxBuf[rxLen] = '\0';
        handleLine(rxBuf, rxLen);
      }
      rxLen = 0;
      rxOverflow = false;
    } else if (rxLen < RX_BUF_SIZE - 1) {
      rxBuf[rxLen++] = c;
    } else {
      rxOverflow = true; // zbyt długa linia - odrzucamy do końca linii
    }
  }
}

void arduinoSendCommand(const uint8_t* payload, size_t length) {
  // znaki sterujące (w tym nowa linia) są odrzucane, aby klient nie mógł wstrzyknąć kilku komend (np. E:CONNECTION)
  if (length < 2 || length > CMD_MAX_LEN) return;
  if (payload[0] != 'W' || payload[1] != ':') return;
  if (!isPrintable(payload, length)) return;

  queueCommand((const char*)payload, length);
  sendQueuedCommand(); // bez opóźnienia, jeśli odstęp od poprzedniej komendy już minął
}

void arduinoSetConnectionStatus(ConnectionStatus status) {
  if (status == connectionStatus) return;
  connectionStatus = status;
  sendConnectionStatus();

  // Arduino odrzuca komendy W: bez połączenia, a po ponownym połączeniu stare komendy byłyby nieaktualne
  if (status != CONN_OK) cmdQueueLen = 0;
}