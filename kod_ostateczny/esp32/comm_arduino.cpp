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
const unsigned long UART_BAUD = 38400;

// bufor sprzętowego UART; Arduino wysyła ~1.5 kB/s, a wysyłanie przez WebSocket do wolnego klienta
// może zablokować loop() nawet na kilka sekund (WEBSOCKETS_TCP_TIMEOUT)
const size_t UART_RX_BUFFER_SIZE = 2048;

// bufor odbiorczy linii
const size_t RX_BUF_SIZE = 64;
static char rxBuf[RX_BUF_SIZE];
static size_t rxLen = 0;
static bool rxOverflow = false;

// ustawiane przez zadanie UART po utracie danych (przepełnienie bufora jest czyszczone przez sterownik)
static volatile bool rxError = false;

// najdłuższa komenda przyjmowana przez Arduino
const size_t CMD_MAX_LEN = 39;

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

void arduinoReceive() {
  if (millis() - lastStatusMs >= STATUS_REPEAT_MS) sendConnectionStatus();

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

  Serial2.write(payload, length);
  Serial2.write('\n');
}

void arduinoSetConnectionStatus(ConnectionStatus status) {
  if (status == connectionStatus) return;
  connectionStatus = status;
  sendConnectionStatus();
}