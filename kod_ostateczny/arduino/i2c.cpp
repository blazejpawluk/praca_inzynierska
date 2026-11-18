/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi magistrali I2C (opis w i2c.h)
*/

#include "i2c.h"
#include <Wire.h>

// konfiguracja
const uint32_t I2C_TIMEOUT_US = 25000; // timeout I2C na wypadek zawieszenia
const uint8_t RECOVERY_CLOCKS = 9; // 8 bitów danych + ACK
const uint8_t HALF_CLOCK_US = 5; // połowa okresu SCL (100 kHz)

static bool faulted = false;

// linie I2C są typu otwarty dren: stan niski jest wymuszany, a wysoki daje podciąganie
static void lineLow(uint8_t pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
  delayMicroseconds(HALF_CLOCK_US);
}

static void lineRelease(uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(HALF_CLOCK_US);
}

// taktuje SCL, aż układ podrzędny zwolni SDA, a następnie generuje STOP
static bool releaseBus() {
  lineRelease(SDA);
  lineRelease(SCL);

  for (uint8_t i = 0; i < RECOVERY_CLOCKS && digitalRead(SDA) == LOW; i++) {
    lineLow(SCL);
    lineRelease(SCL);
  }

  // STOP: zbocze narastające SDA przy wysokim SCL
  lineLow(SCL);
  lineLow(SDA);
  lineRelease(SCL);
  lineRelease(SDA);

  return digitalRead(SDA) == HIGH && digitalRead(SCL) == HIGH;
}

void i2cInit() {
  i2cRecover();
}

bool i2cReady() {
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag();
    faulted = true;
  }
  return !faulted;
}

void i2cMarkFault() {
  faulted = true;
}

bool i2cRecover() {
  Wire.end();
  bool released = releaseBus();

  Wire.begin();
  Wire.setWireTimeout(I2C_TIMEOUT_US, true); // kasuje też flagę timeoutu

  faulted = !released;
  return released;
}
