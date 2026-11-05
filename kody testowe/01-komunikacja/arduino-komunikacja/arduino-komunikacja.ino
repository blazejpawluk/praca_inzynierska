// Błażej Pawluk
// plik testowy komunikacji przez UART z ESP32-CAM, a następnie do strony internetowej z interfejsem użytkownika

// połączenie z ESP32-CAM (przez 2-kierunkowy konwerter poziomów):
// D0 (RX) - IO14
// D1 (TX) - IO13

// co 0,2s wysyła linię z danymi (iteracja przez zbiór 5 losowych zbiorów danych)
// otrzymane informacje wyświetlane tylko na LCD

// wysyłane dane: 
//    D:<enL>,<enR>,<sonar>,<ax>,<ay>,<az>,<gx>,<gy>,<gz>
//    jednostki: enkodery: liczba impulsów, sonar: cm, akcelerometr: g, żyroskop: stopnie/s

// odbierane dane:
//    C:M,<L>,<R> - prędkości silników [-255,255] - ujemna to do tyłu)
//    C:S,<angle> - kąt serwa [45,135]
//    C:TRIG      - wysłanie sygnału TRIG z sonaru

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// konfiguracja UART
const unsigned long BAUD = 38400;
const unsigned long SEND_INTERVAL = 200; // 200ms

// konfiguracja LCD
const uint8_t LCD_ADDR = 0x27;
const uint8_t LCD_COLS = 16;
const uint8_t LCD_ROWS = 2;
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// dane
struct Data {
  int enL, enR;
  int sonar;
  float ax, ay, az;
  float gx, gy, gz;
};

const Data TEST_DATA[5] = { // dane testowe, które będą iterowane (wygenerowane)
  //  encL encR sonar    ax     ay     az      gx     gy     gz
  {      0,   0,  120,  0.00,  0.00,  1.00,   0.0,   0.0,   0.0 },  // stoi
  {     25,  25,   95,  0.12,  0.01,  0.99,   0.5,  -0.3,   0.1 },  // rusza
  {     60,  62,   60,  0.20, -0.02,  0.98,   1.2,   0.4,  -0.8 },  // jedzie prosto
  {     90,  70,   35,  0.05,  0.15,  1.02,   2.0,  -1.5,  12.5 },  // skręca
  {     90,  70,   15, -0.30,  0.00,  0.97,  -0.7,   3.1,   0.2 }   // hamuje przed przeszkodą
};
const uint8_t TEST_COUNT = 5;
uint8_t testIndex = 0;

// odbieranie danych
const uint8_t RX_BUF_SIZE = 64;
char rxBuf[RX_BUF_SIZE];
uint8_t rxLen;
char lastShown[RX_BUF_SIZE];

// wyswietlanie na LCD
void showOnLCD(const char* text) {
  // pomija zapis takiego samego tekstu
  if (strcmp(text, lastShown) == 0) return;

  // lastShown = text
  strncpy(lastShown, text, sizeof(lastShown) - 1);

  size_t len = strlen(text);
  for (uint8_t row = 0; row < LCD_ROWS; row++) {
    lcd.setCursor(0, row);
    for (uint8_t col = 0; col < LCD_COLS; col++) {
      size_t i = (size_t)row * LCD_COLS + col;
      lcd.print(i < len ? text[i] : ' '); // dopisywanie spacji zamiast robienia lcd.clear(), aby usunąć efekt migania
    }
  }
}

// obsługa odebranej linii
void handleLine(const char* line) {
  // TODO: parsowanie poleceń i ich wykonywanie
  showOnLCD(line);
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

  // TODO: zbieranie danych z sensorów

  // dane testowe
  d = TEST_DATA[testIndex];
  testIndex = (testIndex + 1) % TEST_COUNT;

  // wysyłanie danych
  Serial.print(F("D:"));
  Serial.print(d.enL);  Serial.print(',');
  Serial.print(d.enR);  Serial.print(',');
  Serial.print(d.sonar); Serial.print(',');
  Serial.print(d.ax, 2); Serial.print(',');
  Serial.print(d.ay, 2); Serial.print(',');
  Serial.print(d.az, 2); Serial.print(',');
  Serial.print(d.gx, 2); Serial.print(',');
  Serial.print(d.gy, 2); Serial.print(',');
  Serial.println(d.gz, 2);
}

// czas ostatniego wysłanego zestawu danych
unsigned long lastSend = 0;
void setup() {
  Serial.begin(BAUD);

  lcd.init();
  Wire.setClock(400000);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AUTKO gotowe");
  lcd.setCursor(0, 1);
  lcd.print("czekam na dane");

  lastSend = millis();
}

void loop() {
  readSerial();

  if (millis() - lastSend >= SEND_INTERVAL) {
    lastSend = millis();
    sendData();
  }
}