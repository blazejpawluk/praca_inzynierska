// Błażej Pawluk
// komunikacja przez UART z ESP32-CAM: odbieranie poleceń i wysyłanie danych

#include "komunikacja.h"
#include "lcd.h"
#include "enkodery.h"
#include "sonar.h"
#include "serwo.h"
#include "zyroskop.h"

// dane
struct Data {
  unsigned long enL, enR;
  float sonar;
  float ax, ay, az;
  float gx, gy, gz;
};

// odbieranie danych
const uint8_t RX_BUF_SIZE = 64;
char rxBuf[RX_BUF_SIZE];
uint8_t rxLen;

// obsługa odebranej linii
void handleLine(const char* line) {
  showOnLCD(line);

  if (strncmp(line, "C:S,", 4) == 0) {
    setServoAngle(atoi(line + 4));
  } else if (strcmp(line, "C:TRIG") == 0) {
    triggerSonar();
  }
}

// odczytywanie danych z Seriala UART
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

// zbieranie danych i wysyłanie przez Serial UART
void sendData() {
  Data d;

  // zbieranie danych z sensorów
  readEncoders(d.enL, d.enR);
  d.sonar = sonarMm / 10.0;
  d.ax = ax; d.ay = ay; d.az = az;
  d.gx = gx; d.gy = gy; d.gz = gz;

  // wysyłanie danych
  Serial.print(F("D:"));
  Serial.print(d.enL);  Serial.print(',');
  Serial.print(d.enR);  Serial.print(',');
  Serial.print(d.sonar, 1); Serial.print(',');
  Serial.print(d.ax, 2); Serial.print(',');
  Serial.print(d.ay, 2); Serial.print(',');
  Serial.print(d.az, 2); Serial.print(',');
  Serial.print(d.gx, 2); Serial.print(',');
  Serial.print(d.gy, 2); Serial.print(',');
  Serial.println(d.gz, 2);
}
