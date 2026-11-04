/*
 * TEST MPU6050 (GY-521)
 * MPU6050            -> I2C: SDA = A4, SCL = A5 (wspólna szyna z LCD)
 * Lewy enkoder (DO)  -> A0  (ok. 1/4 obrotu lewym kołem  = poprzedni tryb)
 * Prawy enkoder (DO) -> A1  (ok. 1/4 obrotu prawym kołem = następny tryb)
 * LCD I2C 16x2       -> SDA = A4, SCL = A5
 *
 * Po starcie: ~2 s kalibracji żyroskopu - robot musi stać nieruchomo!
 *
 * Tryby (nazwa trybu pokazuje się na 1 s po przełączeniu):
 *   1/4 Akcelerometr - przyspieszenie w osiach X/Y/Z w g + długość wektora |a|
 *   2/4 Zyroskop     - prędkość obrotu w osiach X/Y/Z w °/s
 *   3/4 Katy         - Pi = pochylenie (pitch, + gdy przód w górę),
 *                      Ro = przechylenie (roll, + gdy przechył w prawo),
 *                      Yaw = obrót wokół osi pionowej (+ gdy skręt w lewo,
 *                      zerowany przy wejściu w tryb)
 *   4/4 Diagnostyka  - ID układu, adres I2C, temperatura, liczba błędów I2C
 *
 * Bez dodatkowych bibliotek do MPU - odczyt bezpośrednio z rejestrów.
 * Wymagana biblioteka: "LiquidCrystal I2C" (Frank de Brabander)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// ---------------- KONFIGURACJA ----------------
const uint8_t LCD_ADDR = 0x27;          // jeśli ekran pusty, spróbuj 0x3F

const float ACC_LSB_PER_G = 16384.0;    // zakres ±2 g
const float GYRO_LSB_PER_DPS = 131.0;   // zakres ±250 °/s
const float COMP_ALPHA = 0.98;          // filtr komplementarny (pitch/roll)

// Montaż w robocie: moduł chipem w dół, oś X płytki do tyłu, oś Y w lewo.
// Kod przelicza to na układ robota: X = przód, Y = lewo, Z = góra.
const bool MOUNT_UPSIDE_DOWN = true;    // obrót o 180° wokół osi Y płytki (odwraca X i Z)
const bool CALIBRATE_LEVEL = true;      // przy starcie: aktualne położenie robota = poziom (AX=AY=0)

const unsigned long SAMPLE_MS = 10;     // odczyt czujnika 100 razy/s
const unsigned long LCD_REFRESH_MS = 200;
const unsigned long SPLASH_MS = 1000;   // czas wyświetlania nazwy trybu
const uint16_t CALIB_SAMPLES = 500;

const uint8_t PULSES_TO_SWITCH = 5;     // ile szczelin enkodera = zmiana trybu
const unsigned long DEBOUNCE_US = 2000;
// ----------------------------------------------

// Rejestry MPU6050
const uint8_t REG_SMPLRT_DIV   = 0x19;
const uint8_t REG_CONFIG       = 0x1A;
const uint8_t REG_GYRO_CONFIG  = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;
const uint8_t REG_PWR_MGMT_1   = 0x6B;
const uint8_t REG_WHO_AM_I     = 0x75;

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

uint8_t mpuAddr = 0;                    // wykrywany automatycznie: 0x68 lub 0x69
uint8_t whoAmI = 0;
unsigned int i2cErrors = 0;

// Dane w jednostkach fizycznych
float ax, ay, az;                       // g
float gx, gy, gz;                       // °/s (po odjęciu offsetu)
float tempC;
float gxOff = 0, gyOff = 0, gzOff = 0;
float axOff = 0, ayOff = 0;

float pitch = 0, roll = 0, yaw = 0;

// --- Enkodery jako przełącznik trybów ---
volatile uint8_t pulsesL = 0;
volatile uint8_t pulsesR = 0;
volatile unsigned long lastL = 0;
volatile unsigned long lastR = 0;
volatile uint8_t prevState = 0;

ISR(PCINT1_vect) {
  uint8_t state = PINC & 0b00000011;    // bit0 = A0 (lewy), bit1 = A1 (prawy)
  uint8_t rising = state & ~prevState;
  prevState = state;
  unsigned long now = micros();

  if ((rising & 0b01) && (now - lastL > DEBOUNCE_US)) {
    if (pulsesL < 255) pulsesL++;
    lastL = now;
  }
  if ((rising & 0b10) && (now - lastR > DEBOUNCE_US)) {
    if (pulsesR < 255) pulsesR++;
    lastR = now;
  }
}

const uint8_t MODE_COUNT = 4;
const char* const MODE_NAMES[MODE_COUNT] = {"Akcelerometr", "Zyroskop", "Katy", "Diagnostyka"};
uint8_t mode = 0;
unsigned long splashUntil = 0;

// ---------------- I2C / MPU ----------------
bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(val);
  if (Wire.endTransmission() != 0) { i2cErrors++; return false; }
  return true;
}

bool readRegs(uint8_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) { i2cErrors++; return false; }
  if (Wire.requestFrom(mpuAddr, len) != len) { i2cErrors++; return false; }
  for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
  return true;
}

bool deviceAt(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool initMPU() {
  if (deviceAt(0x68)) mpuAddr = 0x68;
  else if (deviceAt(0x69)) mpuAddr = 0x69;
  else return false;

  readRegs(REG_WHO_AM_I, &whoAmI, 1);

  bool ok = true;
  ok &= writeReg(REG_PWR_MGMT_1, 0x01);   // wybudzenie, zegar z PLL żyroskopu X
  delay(100);
  ok &= writeReg(REG_CONFIG, 0x03);       // filtr DLPF ~44 Hz (tłumi drgania silników)
  ok &= writeReg(REG_SMPLRT_DIV, 0x09);   // 100 Hz
  ok &= writeReg(REG_GYRO_CONFIG, 0x00);  // ±250 °/s
  ok &= writeReg(REG_ACCEL_CONFIG, 0x00); // ±2 g
  return ok;
}

// Odczyt surowy + przeliczenie. withOffset=false używane przy kalibracji.
bool readMPU(bool withOffset = true) {
  uint8_t b[14];
  if (!readRegs(REG_ACCEL_XOUT_H, b, 14)) return false;

  int16_t rax = (b[0] << 8) | b[1];
  int16_t ray = (b[2] << 8) | b[3];
  int16_t raz = (b[4] << 8) | b[5];
  int16_t rt  = (b[6] << 8) | b[7];
  int16_t rgx = (b[8] << 8) | b[9];
  int16_t rgy = (b[10] << 8) | b[11];
  int16_t rgz = (b[12] << 8) | b[13];

  ax = rax / ACC_LSB_PER_G;
  ay = ray / ACC_LSB_PER_G;
  az = raz / ACC_LSB_PER_G;
  tempC = rt / 340.0 + 36.53;
  gx = rgx / GYRO_LSB_PER_DPS;
  gy = rgy / GYRO_LSB_PER_DPS;
  gz = rgz / GYRO_LSB_PER_DPS;

  // Montaż do góry nogami: obrót o 180° wokół osi Y -> odwrócone osie X i Z
  if (MOUNT_UPSIDE_DOWN) {
    ax = -ax; az = -az;
    gx = -gx; gz = -gz;
  }

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
    if (CALIBRATE_LEVEL) {
      axOff = sax / n;
      ayOff = say / n;
    }
  }
}

// Kąty z akcelerometru (w stopniach), układ robota: X = przód, Y = lewo, Z = góra
// Pitch > 0: przód w górę,  Roll > 0: przechył w prawo,  Yaw > 0: skręt w lewo
float accPitch() { return atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI; }
float accRoll()  { return atan2(ay, az) * 180.0 / PI; }

void resetAngles() {
  pitch = accPitch();
  roll = accRoll();
  yaw = 0;
}

void updateAngles(float dt) {
  // przód w górę = obrót ujemny wokół osi Y (lewo), stąd -gy
  pitch = COMP_ALPHA * (pitch - gy * dt) + (1.0 - COMP_ALPHA) * accPitch();
  roll  = COMP_ALPHA * (roll  + gx * dt) + (1.0 - COMP_ALPHA) * accRoll();
  yaw  += gz * dt;
  if (yaw > 180.0)  yaw -= 360.0;
  if (yaw < -180.0) yaw += 360.0;
}

// ---------------- LCD ----------------
void printLine(uint8_t row, const char* text) {
  char line[17];
  snprintf(line, sizeof(line), "%-16s", text);  // dopełnienie spacjami
  lcd.setCursor(0, row);
  lcd.print(line);
}

void showSplash() {
  char line[17];
  snprintf(line, sizeof(line), "%u/%u %s", mode + 1, MODE_COUNT, MODE_NAMES[mode]);
  printLine(0, line);
  printLine(1, mode == 2 ? "Yaw = 0" : "");
}

void showData() {
  char a[8], b[8], line[17];

  switch (mode) {
    case 0:  // Akcelerometr [g]
      dtostrf(ax, 5, 2, a); dtostrf(ay, 5, 2, b);
      snprintf(line, sizeof(line), "AX%s AY%s", a, b);
      printLine(0, line);
      dtostrf(az, 5, 2, a);
      dtostrf(sqrt(ax * ax + ay * ay + az * az), 5, 2, b);
      snprintf(line, sizeof(line), "AZ%s |a|%s", a, b);
      printLine(1, line);
      break;

    case 1:  // Żyroskop [°/s]
      dtostrf(gx, 6, 1, a); dtostrf(gy, 6, 1, b);
      snprintf(line, sizeof(line), "X%s Y%s", a, b);
      printLine(0, line);
      dtostrf(gz, 6, 1, a);
      snprintf(line, sizeof(line), "Z%s st/s", a);
      printLine(1, line);
      break;

    case 2:  // Kąty [°]
      dtostrf(pitch, 5, 1, a); dtostrf(roll, 6, 1, b);
      snprintf(line, sizeof(line), "Pi%s Ro%s", a, b);
      printLine(0, line);
      dtostrf(yaw, 7, 1, a);
      snprintf(line, sizeof(line), "Yaw%s st", a);
      printLine(1, line);
      break;

    case 3:  // Diagnostyka
      snprintf(line, sizeof(line), "ID 0x%02X adr 0x%02X", whoAmI, mpuAddr);
      printLine(0, line);
      dtostrf(tempC, 5, 1, a);
      snprintf(line, sizeof(line), "T%sC Err%4u", a, i2cErrors);
      printLine(1, line);
      break;
  }
}

// ---------------- Przełączanie trybów ----------------
void checkModeSwitch(unsigned long now) {
  noInterrupts();
  uint8_t l = pulsesL;
  uint8_t r = pulsesR;
  interrupts();

  int8_t change = 0;
  if (r >= PULSES_TO_SWITCH) change = 1;
  else if (l >= PULSES_TO_SWITCH) change = -1;
  if (change == 0) return;

  noInterrupts();
  pulsesL = 0;
  pulsesR = 0;
  interrupts();

  mode = (mode + MODE_COUNT + change) % MODE_COUNT;
  if (mode == 2) resetAngles();
  splashUntil = now + SPLASH_MS;
  showSplash();
}

// ---------------- Główny program ----------------
unsigned long lastSampleMs = 0;
unsigned long lastLcdMs = 0;

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);

  Wire.begin();
  Wire.setClock(100000);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);      // zabezpieczenie przed zawieszeniem I2C
#endif

  lcd.init();
  lcd.backlight();
  printLine(0, "Test MPU6050");
  printLine(1, "Szukam ukladu..");
  delay(800);

  if (!initMPU()) {
    printLine(0, "MPU NIE WYKRYTO");
    printLine(1, "Sprawdz SDA/SCL");
    while (true) {}                     // zatrzymanie - sprawdź połączenia i zresetuj
  }

  calibrateGyro();
  readMPU();
  resetAngles();

  noInterrupts();
  prevState = PINC & 0b00000011;
  PCICR  |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT8) | (1 << PCINT9);
  interrupts();

  lastSampleMs = millis();
  splashUntil = millis() + SPLASH_MS;
  showSplash();
}

void loop() {
  unsigned long now = millis();

  checkModeSwitch(now);

  if (now - lastSampleMs >= SAMPLE_MS) {
    float dt = (now - lastSampleMs) / 1000.0;
    lastSampleMs = now;
    if (readMPU()) updateAngles(dt);
  }

  if (now >= splashUntil && now - lastLcdMs >= LCD_REFRESH_MS) {
    lastLcdMs = now;
    showData();
  }
}