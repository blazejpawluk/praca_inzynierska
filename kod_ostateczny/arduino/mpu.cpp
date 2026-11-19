/*
  Błażej Pawluk

  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Implementacja obsługi MPU6050 (opis w mpu.h)
*/

#include "mpu.h"
#include "i2c.h"
#include <Wire.h>

const uint8_t MPU_ADDR = 0x68;

// rejestry
const uint8_t MPU_REG_SMPLRT_DIV   = 0x19; // dzielnik częstotliwości próbkowania
const uint8_t MPU_REG_CONFIG       = 0x1A; // filtr dolnoprzepustowy (DLPF)
const uint8_t MPU_REG_GYRO_CONFIG  = 0x1B; // zakres żyroskopu
const uint8_t MPU_REG_ACCEL_CONFIG = 0x1C; // zakres akcelerometru
const uint8_t MPU_REG_INT_ENABLE   = 0x38; // włączenie przerwań
const uint8_t MPU_REG_INT_STATUS   = 0x3A; // flagi przerwań (m.in. DATA_RDY)
const uint8_t MPU_REG_ACCEL_XOUT_H = 0x3B; // początek bloku 14 bajtów danych
const uint8_t MPU_REG_GYRO_XOUT_H  = 0x43; // żyroskop, początek bloku 6 bajtów osi X, Y, Z
const uint8_t MPU_REG_GYRO_ZOUT_H  = 0x47; // żyroskop, oś Z
const uint8_t MPU_REG_PWR_MGMT_1   = 0x6B; // zasilanie, reset, źródło zegara
const uint8_t MPU_REG_PWR_MGMT_2   = 0x6C; // usypianie pojedynczych osi
const uint8_t MPU_REG_WHO_AM_I     = 0x75; // identyfikator układu

// wartości i bity rejestrów
const uint8_t MPU_PWR1_DEVICE_RESET  = 0x80; // PWR_MGMT_1: reset układu
const uint8_t MPU_PWR1_CLK_PLL_XGYRO = 0x01; // PWR_MGMT_1: zegar z PLL osi X żyroskopu (stabilniejszy niż wewnętrzny)
const uint8_t MPU_PWR2_ALL_AXES_ON   = 0x00; // PWR_MGMT_2: żadna oś nie jest uśpiona
const uint8_t MPU_INT_DATA_RDY       = 0x01; // INT_ENABLE / INT_STATUS: bit DATA_RDY
const uint8_t MPU_FS_SEL_SHIFT       = 3;    // pozycja pola FS_SEL / AFS_SEL w GYRO_CONFIG / ACCEL_CONFIG
const uint8_t MPU_FS_SEL_MASK        = 0x03;
const uint8_t MPU_DLPF_MASK          = 0x07;

// wartości WHO_AM_I (niezależne od adresu I2C ustawianego pinem AD0)
const uint8_t MPU_WHO_AM_I_6050   = 0x68; // oryginalny MPU6050
const uint8_t MPU_WHO_AM_I_6500   = 0x70; // MPU6500 montowany w części modułów GY-521
const uint8_t MPU_WHO_AM_I_CLONE1 = 0x72; // zamiennik MPU6050
const uint8_t MPU_WHO_AM_I_CLONE2 = 0x98; // zamiennik MPU6050

// ustawienia
const uint8_t MPU_ACCEL_FS   = 0; // zakres akcelerometru [+-2g]
const uint8_t MPU_GYRO_FS    = 0; // zakres żyroskopu [+-250 stopni/s]
const uint8_t MPU_DLPF_CFG   = 3; // filtr DLPF (szumu)
const uint8_t MPU_SMPLRT_DIV = MPU_SAMPLE_US / 1000 - 1; // 1 kHz (przy włączonym DLPF) / (1 + dzielnik)
const unsigned long MPU_RESET_MS = 100;   // czas na reset układu
const unsigned long MPU_STARTUP_MS = 100; // czas na ustabilizowanie żyroskopu

// przeliczniki (z dokumentacji MPU6050)
const long MPU_ACCEL_LSB_PER_G = 16384L >> MPU_ACCEL_FS;
const long MPU_GYRO_LSB_PER_DPS_FS0 = 131; // dla zakresu +-250 stopni/s; wyższe zakresy dzielą go przez 2^FS
const float MPU_GYRO_LSB_PER_DPS = (float)MPU_GYRO_LSB_PER_DPS_FS0 / (1 << MPU_GYRO_FS);
const long MPU_TEMP_LSB_PER_C = 340;
const long MPU_TEMP_OFFSET_C_X100 = 3653;  // 36.53 stopnia C

const unsigned long MPU_CALIB_EXTRA_MS = 100;

// offset żyroskopu
static float gyroOffset[3] = {0, 0, 0}; // X, Y, Z

// zapisuje bajt do rejestru i zwraca true jeśli układ potwierdził
static bool writeReg(uint8_t reg, uint8_t value) {
  if (!i2cReady()) return false;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// odczyt len kolejnych rejestrów od reg
static bool readRegs(uint8_t reg, uint8_t* buf, uint8_t len) {
  if (!i2cReady()) return false;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false; // repeated START
  if (Wire.requestFrom(MPU_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

// składa 2 bajty w liczbę
static int16_t toInt16(uint8_t hi, uint8_t lo) {
  return (int16_t)(((uint16_t)hi << 8) | lo);
}

// true, jeśli pod adresem MPU_ADDR odpowiada MPU6050 lub zgodny zamiennik
static bool isPresent() {
  uint8_t id;
  if (!readRegs(MPU_REG_WHO_AM_I, &id, 1)) return false;
  return id == MPU_WHO_AM_I_6050 || id == MPU_WHO_AM_I_6500 ||
         id == MPU_WHO_AM_I_CLONE1 || id == MPU_WHO_AM_I_CLONE2;
}

bool mpuConfigure() {
  if (!isPresent()) return false;
  if (!writeReg(MPU_REG_PWR_MGMT_1, MPU_PWR1_DEVICE_RESET)) return false;
  delay(MPU_RESET_MS);

  bool ok = true;
  ok &= writeReg(MPU_REG_PWR_MGMT_1, MPU_PWR1_CLK_PLL_XGYRO);
  ok &= writeReg(MPU_REG_PWR_MGMT_2, MPU_PWR2_ALL_AXES_ON);
  ok &= writeReg(MPU_REG_CONFIG, MPU_DLPF_CFG & MPU_DLPF_MASK);
  ok &= writeReg(MPU_REG_SMPLRT_DIV, MPU_SMPLRT_DIV);
  ok &= writeReg(MPU_REG_GYRO_CONFIG, (MPU_GYRO_FS & MPU_FS_SEL_MASK) << MPU_FS_SEL_SHIFT);
  ok &= writeReg(MPU_REG_ACCEL_CONFIG, (MPU_ACCEL_FS & MPU_FS_SEL_MASK) << MPU_FS_SEL_SHIFT);
  ok &= writeReg(MPU_REG_INT_ENABLE, MPU_INT_DATA_RDY);

  delay(MPU_STARTUP_MS);
  return ok;
}

bool mpuCalibrate(uint16_t samples) {
  if (samples == 0) return false;

  // sumy surowych odczytów; long wystarcza nawet dla 65535 próbek o wartości 32767
  long sum[3] = {0, 0, 0};
  uint16_t count = 0;

  unsigned long timeoutMs = 2UL * samples * MPU_SAMPLE_US / 1000 + MPU_CALIB_EXTRA_MS;
  unsigned long startMs = millis();

  // każda próbka jest brana dopiero po ustawieniu flagi DATA_RDY, aby nie liczyć tej samej próbki kilka razy
  while (count < samples) {
    if (!i2cReady() || millis() - startMs > timeoutMs) return false;
    if (!mpuDataReady()) continue;

    uint8_t b[6];
    if (!readRegs(MPU_REG_GYRO_XOUT_H, b, 6)) continue; // pojedynczy błąd odczytu pomijamy
    sum[0] += toInt16(b[0], b[1]);
    sum[1] += toInt16(b[2], b[3]);
    sum[2] += toInt16(b[4], b[5]);
    count++;
  }

  // nowy offset zapisujemy dopiero po zebraniu wszystkich próbek
  for (uint8_t i = 0; i < 3; i++) gyroOffset[i] = (float)sum[i] / count;
  return true;
}

bool mpuDataReady() {
  uint8_t status;
  if (!readRegs(MPU_REG_INT_STATUS, &status, 1)) return false;
  return status & MPU_INT_DATA_RDY;
}

bool mpuRead(MpuData& data) {
  uint8_t b[14];
  if (!readRegs(MPU_REG_ACCEL_XOUT_H, b, 14)) return false;

  // odwrócone osie X i Z – moduł zamontowany do góry nogami
  int16_t ax   = -toInt16(b[0],  b[1]);
  int16_t ay   =  toInt16(b[2],  b[3]);
  int16_t az   = -toInt16(b[4],  b[5]);
  int16_t temp =  toInt16(b[6],  b[7]);
  long gx      = -lround(toInt16(b[8],  b[9])  - gyroOffset[0]);
  long gy      =  lround(toInt16(b[10], b[11]) - gyroOffset[1]);
  long gz      = -lround(toInt16(b[12], b[13]) - gyroOffset[2]);

  // przeliczenie na liczby całkowite (bez float)
  data.ax = (long)ax * 1000 / MPU_ACCEL_LSB_PER_G;
  data.ay = (long)ay * 1000 / MPU_ACCEL_LSB_PER_G;
  data.az = (long)az * 1000 / MPU_ACCEL_LSB_PER_G;
  data.gx = gx * (100L << MPU_GYRO_FS) / MPU_GYRO_LSB_PER_DPS_FS0;
  data.gy = gy * (100L << MPU_GYRO_FS) / MPU_GYRO_LSB_PER_DPS_FS0;
  data.gz = gz * (100L << MPU_GYRO_FS) / MPU_GYRO_LSB_PER_DPS_FS0;
  data.temp = (long)temp * 100 / MPU_TEMP_LSB_PER_C + MPU_TEMP_OFFSET_C_X100;
  return true;
}

bool mpuReadGz(float& gz) {
  uint8_t b[2];
  if (!readRegs(MPU_REG_GYRO_ZOUT_H, b, 2)) return false;
  gz = -(toInt16(b[0], b[1]) - gyroOffset[2]) / MPU_GYRO_LSB_PER_DPS;
  return true;
}