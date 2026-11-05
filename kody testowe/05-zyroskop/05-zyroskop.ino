// Błażej Pawluk
// test żyroskopu i akcelerometru
// akcelerometr: x - przód, y - lewo, z - góra
// żyroskop: x - prawo w dół, y - przód w dół, z - skręt w lewo

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// konfiguracja LCD
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// konfiguracja MPU
uint8_t mpuAddr = 0;
uint8_t whoAmI = 0;
const uint16_t CALIB_SAMPLES = 500;
const float ACC_LSB_PER_G = 16384.0; // zakres +-2 g
const float GYRO_LSB_PER_DPS = 131.0; // zakres +-250 stopni/s

// rejestry MPU
const uint8_t REG_WHO_AM_I = 0x75;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_SMPLRT_DIV = 0x19;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;

float ax, ay, az; // g
float gx, gy, gz; // stopnie/s (po odjęciu offsetu)
float tempC;
float gxOff = 0, gyOff = 0, gzOff = 0;
float axOff = 0, ayOff = 0;

const unsigned long SAMPLE_MS = 10;
const unsigned long LCD_REFRESH_MS = 200;

bool deviceAt(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool readRegs(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddr, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(val);
  if (Wire.endTransmission() != 0) return false;
  return true;
}

bool initMPU() {
  if (deviceAt(0x68)) mpuAddr = 0x68;
  else if (deviceAt(0x69)) mpuAddr = 0x69;
  else return false;

  readRegs(REG_WHO_AM_I, &whoAmI, 1);

  bool ok = true;
  ok &= writeReg(REG_PWR_MGMT_1, 0x01); // wybudzenie, zegar z PLL żyroskopu X
  delay(100);
  ok &= writeReg(REG_CONFIG, 0x03); // filtr DLPF ~44 Hz (tłumi drgania silników)
  ok &= writeReg(REG_SMPLRT_DIV, 0x09); // 100 Hz
  ok &= writeReg(REG_GYRO_CONFIG, 0x00); // +-250 stopni/s
  ok &= writeReg(REG_ACCEL_CONFIG, 0x00); // +-2 g
  return ok;
}

bool readMPU(bool withOffset = true) {
  uint8_t b[14];
  if (!readRegs(REG_ACCEL_XOUT_H, b, 14)) return false;

  int16_t rax = (b[0]  << 8) | b[1];
  int16_t ray = (b[2]  << 8) | b[3];
  int16_t raz = (b[4]  << 8) | b[5];
  int16_t rt  = (b[6]  << 8) | b[7];
  int16_t rgx = (b[8]  << 8) | b[9];
  int16_t rgy = (b[10] << 8) | b[11];
  int16_t rgz = (b[12] << 8) | b[13];

  // moduł zamontowany chipem w dół: osie czujnika X - tył, Y - lewo, Z - dół
  // odwrócenie X i Z daje układ robota: X - przód, Y - lewo, Z - góra
  ax = -rax / ACC_LSB_PER_G;
  ay = ray / ACC_LSB_PER_G;
  az = -raz / ACC_LSB_PER_G;
  tempC = rt / 340.0 + 36.53;
  gx = -rgx / GYRO_LSB_PER_DPS;
  gy = rgy / GYRO_LSB_PER_DPS;
  gz = -rgz / GYRO_LSB_PER_DPS;

  if (withOffset) {
    ax -= axOff;
    ay -= ayOff;
    gx -= gxOff;
    gy -= gyOff;
    gz -= gzOff;
  }
  return true;
}

void calibrateGyro() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Kalibracja...");
  lcd.setCursor(0, 1);
  lcd.print("NIE RUSZAJ!");

  float sx = 0, sy = 0, sz = 0, sax = 0, say = 0;
  uint16_t n = 0;
  for (uint16_t i = 0; i < CALIB_SAMPLES; i++) {
    if (readMPU(false)) {
      sx += gx; sy += gy; sz += gz;
      sax += ax; say += ay;
      n++;
    }
    delay(3);
  }
  if (n > 0) {
    gxOff = sx / n;
    gyOff = sy / n;
    gzOff = sz / n;
    axOff = sax / n;
    ayOff = say / n;
  }
}

void showData() {
  char x[8], y[8], z[8];
  char line[LCD_COLS + 1];

  dtostrf(ax, 4, 1, x);
  dtostrf(ay, 4, 1, y);
  dtostrf(az, 4, 1, z);
  snprintf(line, sizeof(line), "A%s %s %s ", x, y, z);
  lcd.setCursor(0, 0);
  lcd.print(line);

  snprintf(line, sizeof(line), "G%4d %4d %4d ", (int)round(gx), (int)round(gy), (int)round(gz));
  lcd.setCursor(0, 1);
  lcd.print(line);
}

unsigned long lastSampleMs = 0;
unsigned long lastLcdMs = 0;
void setup() {
  Wire.begin();
  Wire.setClock(100000);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  initMPU();
  calibrateGyro();

  lastSampleMs = millis();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleMs >= SAMPLE_MS) {
    lastSampleMs = now;
    readMPU();
  }
  if (now - lastLcdMs >= LCD_REFRESH_MS) {
    lastLcdMs = now;
    showData();
  }
}