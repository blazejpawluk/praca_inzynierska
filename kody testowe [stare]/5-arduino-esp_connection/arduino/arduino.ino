// Błażej Pawluk

#include "Wheels.h"
#include "PinChangeInterrupt.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SENSOR_LEFT_PIN A0
#define SENSOR_RIGHT_PIN A1

// LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2);

// orange, blue, yellow, green, brown, purple
Wheels w(3, 6, 4, 5, 7, 8);

// sensor counters
volatile int sensorL, sensorR;

void setup() {

  Serial.begin(115200);

  pinMode(SENSOR_LEFT_PIN, INPUT);
  pinMode(SENSOR_RIGHT_PIN, INPUT);

  sensorL = 0;
  sensorR = 0;

  // I2C LCD
  lcd.init();
  lcd.backlight();

  // Wyświetlenie wartości początkowej
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("0,0");

  // Przerwania dla A0 i A1
  PCICR = 0x02;
  PCMSK1 = 0x03;

  // w.forward();
}

void loop() {

  // Zbudowanie dokładnie tego samego tekstu,
  // który zostanie wysłany przez Serial
  String data = String(sensorL) + "," + String(sensorR);

  // UART
  Serial.println(data);

  // LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(data);

  delay(250);
}

ISR(PCINT1_vect) {

  if (PINC & (1 << PC0)) {
    sensorL++;
  }

  if (PINC & (1 << PC1)) {
    sensorR++;
  }
}