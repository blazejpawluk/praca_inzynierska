/*
  AUTKO - Arduino UNO

  - co 1 s wysyła linię telemetrii przez Serial (D1 = TX -> ESP32-CAM)
  - dane są na razie testowe: program przechodzi cyklicznie między 5 zestawami
  - każdą odebraną linię (D0 = RX <- ESP32-CAM) wyświetla na LCD (bez parsowania)

  Protokół (tekst, każda linia kończy się \n):

    Arduino -> strona:
      T:<encL>,<encR>,<sonar_cm>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>
        enkodery: liczba impulsów, sonar: cm, akcelerometr: g, żyroskop: °/s

    strona -> Arduino:
      C:M,<lewy>,<prawy>   prędkości silników -255..255 (ujemna = do tyłu)
      C:S,<kąt>            kąt serwa 0..180
      C:TRIG               jeden pomiar sonaru

  UWAGA: D0/D1 to ten sam Serial co USB. Na czas wgrywania programu
  odłącz przewód D0 (RX) od ESP32, bo inaczej wgrywanie się nie uda.
  Nie używaj Serial.print do debugowania - wszystko, co wyślesz, trafi do strony.

  Biblioteka: "LiquidCrystal I2C" (Frank de Brabander).
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- Konfiguracja ----------
const unsigned long BAUD = 115200;
const unsigned long SEND_INTERVAL_MS = 1000;

const uint8_t LCD_ADDR = 0x27;   // jeśli LCD nie działa, spróbuj 0x3F
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ---------- Dane ----------
struct Sample {
  int encL;
  int encR;
  int sonar;
  float ax, ay, az;
  float gx, gy, gz;
};

// 5 stałych zestawów testowych, wysyłanych po kolei w pętli
const Sample TEST_DATA[5] = {
  //  encL encR sonar    ax     ay     az      gx     gy     gz
  {      0,   0,  120,  0.00,  0.00,  1.00,   0.0,   0.0,   0.0 },  // stoi
  {     25,  25,   95,  0.12,  0.01,  0.99,   0.5,  -0.3,   0.1 },  // rusza
  {     60,  62,   60,  0.20, -0.02,  0.98,   1.2,   0.4,  -0.8 },  // jedzie prosto
  {     90,  70,   35,  0.05,  0.15,  1.02,   2.0,  -1.5,  12.5 },  // skręca
  {     90,  70,   15, -0.30,  0.00,  0.97,  -0.7,   3.1,   0.2 }   // hamuje przed przeszkodą
};
const uint8_t TEST_COUNT = 5;
uint8_t testIndex = 0;

unsigned long lastSend = 0;

// ---------- Odbiór ----------
const uint8_t RX_BUF_SIZE = 64;
char rxBuf[RX_BUF_SIZE];
uint8_t rxLen = 0;
char lastShown[RX_BUF_SIZE] = "";

// ---------- LCD ----------
// Wypisuje tekst na całym ekranie (zawijanie co LCD_COLS znaków).
// Zamiast lcd.clear() dopełnia spacjami - ekran nie miga.
// Pomija zapis, jeśli tekst jest taki sam jak poprzednio (I2C jest wolne).
void showOnLcd(const char* text) {
  if (strcmp(text, lastShown) == 0) return;
  strncpy(lastShown, text, sizeof(lastShown) - 1);

  size_t len = strlen(text);
  for (uint8_t row = 0; row < LCD_ROWS; row++) {
    lcd.setCursor(0, row);
    for (uint8_t col = 0; col < LCD_COLS; col++) {
      size_t i = (size_t)row * LCD_COLS + col;
      lcd.print(i < len ? text[i] : ' ');
    }
  }
}

// ---------- Obsługa odebranej linii ----------
void handleLine(const char* line) {
  // ===== MIEJSCE NA PARSOWANIE I REAKCJĘ NA POLECENIA ZE STRONY =====
  // Tu rozpoznaj typ polecenia i wykonaj akcję, np.:
  //   "C:M,<l>,<p>"  -> ustaw prędkości silników przez L298N (PWM + piny kierunku)
  //   "C:S,<kąt>"    -> servo.write(kąt)
  //   "C:TRIG"       -> wyślij impuls na TRIG sonaru i odczytaj ECHO
  // Przy sterowaniu silnikami dodaj watchdog: brak polecenia C:M przez ~500 ms
  // = zatrzymanie silników (strona powtarza C:M co 200 ms, dopóki jedziesz).
  // Wyświetlanie na LCD zostaw najwyżej jako podgląd debugowy.
  // =====================================================================

  showOnLcd(line);
}

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
    // zbyt długa linia jest obcinana
  }
}

// ---------- Wysyłanie ----------
void sendTelemetry() {
  Sample s;

  // ===== MIEJSCE NA ZBIERANIE DANYCH Z CZUJNIKÓW (obecnie wyłączone) =====
  // s.encL  = ...;                  // enkoder lewy  (A0)
  // s.encR  = ...;                  // enkoder prawy (A1)
  // s.sonar = ...;                  // sonar HC-SR04 (A2/A3), cm
  // s.ax = ...; s.ay = ...; s.az = ...;   // MPU6050, g
  // s.gx = ...; s.gy = ...; s.gz = ...;   // MPU6050, °/s
  // =======================================================================

  // Dane testowe (usuń, gdy włączysz zbieranie danych powyżej)
  s = TEST_DATA[testIndex];
  testIndex = (testIndex + 1) % TEST_COUNT;

  Serial.print(F("T:"));
  Serial.print(s.encL);  Serial.print(',');
  Serial.print(s.encR);  Serial.print(',');
  Serial.print(s.sonar); Serial.print(',');
  Serial.print(s.ax, 2); Serial.print(',');
  Serial.print(s.ay, 2); Serial.print(',');
  Serial.print(s.az, 2); Serial.print(',');
  Serial.print(s.gx, 2); Serial.print(',');
  Serial.print(s.gy, 2); Serial.print(',');
  Serial.println(s.gz, 2);
}

void setup() {
  Serial.begin(BAUD);

  lcd.init();
  Wire.setClock(400000);   // szybszy I2C = krótsze zapisy na LCD
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AUTKO gotowe");
  lcd.setCursor(0, 1);
  lcd.print("czekam na dane");

  lastSend = millis();
}

void loop() {
  readSerial();

  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    sendTelemetry();
  }
}
