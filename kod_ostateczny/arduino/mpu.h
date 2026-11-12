/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Biblioteka obsługująca odbieranie danych z modułu MPU6050 z akcelerometrem i żyroskopem

  Połączenie: SDA=SDA, SCL=SCL
*/

#pragma once

#include <Arduino.h>
#include <Wire.h>

// rejestry
#define MPU_ADDR 0x68
#define MPU_REG_SMPLRT_DIV   0x19 // dzielnik częstotliwości próbkowania
#define MPU_REG_CONFIG       0x1A // filtr dolnoprzepustowy (DLPF)
#define MPU_REG_GYRO_CONFIG  0x1B // zakres żyroskopu
#define MPU_REG_ACCEL_CONFIG 0x1C // zakres akcelerometru
#define MPU_REG_INT_ENABLE   0x38 // włączenie przerwań
#define MPU_REG_INT_STATUS   0x3A // flagi przerwań (m.in. DATA_RDY)
#define MPU_REG_ACCEL_XOUT_H 0x3B // początek bloku 14 bajtów danych
#define MPU_REG_TEMP_OUT_H   0x41 // temperatura
#define MPU_REG_GYRO_XOUT_H  0x43 // żyroskop
#define MPU_REG_PWR_MGMT_1   0x6B // zasilanie, reset, źródło zegara
#define MPU_REG_PWR_MGMT_2   0x6C // usypianie pojedynczych osi
#define MPU_REG_WHO_AM_I     0x75 // identyfikator układu

// ustawienia
#define MPU_ACCEL_FS         0    // zakres akcelerometru [+-2g]
#define MPU_GYRO_FS          0    // zakres żyroskopu [+- 250 stopni/s]
#define MPU_DLPF_CFG         3    // filtr DLPF (szumu)
#define MPU_SMPLRT_DIV       4    // częstotliwość próbkowania (200 Hz, próbki co 5ms)
#define MPU_ACCEL_LSB_PER_G  (16384.0f / (1 << MPU_ACCEL_FS)) // czułość LSB na 1 g
#define MPU_GYRO_LSB_PER_DPS (131.0f / (1 << MPU_GYRO_FS))    // czułość LSB na 1 °/s

// zapisuje bajt do rejestru i zwraca true jeśli układ potwierdził
bool mpuWriteReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// odczyt rejestrów
bool mpuReadRegs(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;   // repeated START
  if (Wire.requestFrom((uint8_t)MPU_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

// składa 2 bajty w liczbę
int16_t mpuToInt16(uint8_t hi, uint8_t lo) {
  return (int16_t)(((uint16_t)hi << 8) | lo);
}

uint8_t whoAmIMPU() {
  uint8_t id;
  if (!mpuReadRegs(MPU_REG_WHO_AM_I, &id, 1)) return 0xFF;
  return id;
}

bool initMPU() {
  Wire.begin();
  Wire.setClock(100000);
  Wire.setWireTimeout(3000, true);
 
  if (!mpuWriteReg(MPU_REG_PWR_MGMT_1, 0x80)) return false;
  delay(100);
 
  bool ok = true;
  ok &= mpuWriteReg(MPU_REG_PWR_MGMT_1, 0x01);
  ok &= mpuWriteReg(MPU_REG_PWR_MGMT_2, 0x00);
  ok &= mpuWriteReg(MPU_REG_CONFIG, MPU_DLPF_CFG & 0x07);
  ok &= mpuWriteReg(MPU_REG_SMPLRT_DIV, MPU_SMPLRT_DIV);
  ok &= mpuWriteReg(MPU_REG_GYRO_CONFIG, (MPU_GYRO_FS & 0x03) << 3);
  ok &= mpuWriteReg(MPU_REG_ACCEL_CONFIG, (MPU_ACCEL_FS & 0x03) << 3);
  ok &= mpuWriteReg(MPU_REG_INT_ENABLE, 0x01);
 
  delay(100);
  return ok;
}

bool dataReadyMPU() {
  uint8_t status;
  if (!mpuReadRegs(MPU_REG_INT_STATUS, &status, 1)) return false;
  return status & 0x01;
}

bool readMPURaw(int16_t &ax, int16_t &ay, int16_t &az,
                int16_t &gx, int16_t &gy, int16_t &gz,
                int16_t &temp) {
  uint8_t b[14];
  if (!mpuReadRegs(MPU_REG_ACCEL_XOUT_H, b, 14)) return false;
  ax   = -mpuToInt16(b[0],  b[1]);
  ay   = mpuToInt16(b[2],  b[3]);
  az   = -mpuToInt16(b[4],  b[5]);
  temp = mpuToInt16(b[6],  b[7]);
  gx   = -mpuToInt16(b[8],  b[9]);
  gy   = mpuToInt16(b[10], b[11]);
  gz   = -mpuToInt16(b[12], b[13]);
  return true;
}

bool readMPU(float &ax, float &ay, float &az,
             float &gx, float &gy, float &gz,
             float &temp) {
  int16_t rax, ray, raz, rgx, rgy, rgz, rt;
  if (!readMPURaw(rax, ray, raz, rgx, rgy, rgz, rt)) return false;
  ax   = rax / MPU_ACCEL_LSB_PER_G;
  ay   = ray / MPU_ACCEL_LSB_PER_G;
  az   = raz / MPU_ACCEL_LSB_PER_G;
  gx   = rgx / MPU_GYRO_LSB_PER_DPS;
  gy   = rgy / MPU_GYRO_LSB_PER_DPS;
  gz   = rgz / MPU_GYRO_LSB_PER_DPS;
  temp = rt / 340.0f + 36.53f;
  return true;
}

bool readGzRaw(int16_t &gz) {
  uint8_t b[2];
  if (!mpuReadRegs(MPU_REG_GYRO_XOUT_H + 4, b, 2)) return false;
  gz = -mpuToInt16(b[0], b[1]);
  return true;
}
 
bool readGz(float &gz) {
  int16_t raw;
  if (!readGzRaw(raw)) return false;
  gz = raw / MPU_GYRO_LSB_PER_DPS;
  return true;
}