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
#define MPU_REG_GYRO_XOUT_H  0x43 // żyroskop
#define MPU_REG_PWR_MGMT_1   0x6B // zasilanie, reset, źródło zegara
#define MPU_REG_PWR_MGMT_2   0x6C // usypianie pojedynczych osi
#define MPU_REG_WHO_AM_I     0x75 // identyfikator układu

// wartości WHO_AM_I (niezależne od adresu I2C ustawianego pinem AD0)
#define MPU_WHO_AM_I_6050    0x68 // oryginalny MPU6050
#define MPU_WHO_AM_I_6500    0x70 // MPU6500 montowany w części modułów GY-521
#define MPU_WHO_AM_I_CLONE1  0x72 // zamiennik MPU6050
#define MPU_WHO_AM_I_CLONE2  0x98 // zamiennik MPU6050

// ustawienia
#define MPU_ACCEL_FS         0    // zakres akcelerometru [+-2g]
#define MPU_GYRO_FS          0    // zakres żyroskopu [+- 250 stopni/s]
#define MPU_DLPF_CFG         3    // filtr DLPF (szumu)
#define MPU_SMPLRT_DIV       4    // częstotliwość próbkowania (200 Hz, próbki co 5ms)
#define MPU_SAMPLE_US        ((MPU_SMPLRT_DIV + 1) * 1000UL) // okres próbki (1 kHz przy włączonym DLPF / (1 + dzielnik))
#define MPU_POLL_US          (MPU_SAMPLE_US * 9 / 10)        // od ostatniego odczytu pytamy o nową próbkę dopiero po tym czasie
                                                             // (90%, aby opóźnienia pętli nie powodowały gubienia próbek)
#define MPU_ACCEL_LSB_PER_G_INT (16384L >> MPU_ACCEL_FS)      // czułość LSB na 1 g
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

bool isMPU() {
  uint8_t id = whoAmIMPU();
  return id == MPU_WHO_AM_I_6050 || id == MPU_WHO_AM_I_6500 ||
         id == MPU_WHO_AM_I_CLONE1 || id == MPU_WHO_AM_I_CLONE2;
}

bool initMPU() {
  if (!isMPU()) return false;
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

// odczyt przeskalowany do liczb całkowitych (bez float):
// przyspieszenie [mg], prędkość kątowa [0.01 stopnia/s], temperatura [0.01 stopnia C]
bool readMPU(int16_t &ax, int16_t &ay, int16_t &az,
             long &gx, long &gy, long &gz,
             int16_t &temp) {
  int16_t rax, ray, raz, rgx, rgy, rgz, rt;
  if (!readMPURaw(rax, ray, raz, rgx, rgy, rgz, rt)) return false;
  ax   = (long)rax * 1000 / MPU_ACCEL_LSB_PER_G_INT;
  ay   = (long)ray * 1000 / MPU_ACCEL_LSB_PER_G_INT;
  az   = (long)raz * 1000 / MPU_ACCEL_LSB_PER_G_INT;
  gx   = (long)rgx * (100L << MPU_GYRO_FS) / 131;
  gy   = (long)rgy * (100L << MPU_GYRO_FS) / 131;
  gz   = (long)rgz * (100L << MPU_GYRO_FS) / 131;
  temp = (long)rt * 100 / 340 + 3653;
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