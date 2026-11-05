// Błażej Pawluk
// obsługa silników DC przez mostek H L298N

#include "silniki.h"

// piny mostka H (ENA, ENB - PWM na Timer2 i Timer0, Timer1 zajęty przez serwo)
const uint8_t ENA_PIN = 3; // lewy, prędkość
const uint8_t IN1_PIN = 4; // lewy, kierunek
const uint8_t IN2_PIN = 5;
const uint8_t ENB_PIN = 6; // prawy, prędkość
const uint8_t IN3_PIN = 7; // prawy, kierunek
const uint8_t IN4_PIN = 8;

const int MAX_SPEED = 255;
const unsigned long MOTOR_TIMEOUT = 1000; // 1s bez polecenia - zatrzymanie (np. utrata Wi-Fi)

int speedL = 0, speedR = 0;
unsigned long lastCmd = 0;

// ustawienie jednego koła: kierunek na pinach IN, prędkość PWM na pinie EN
void setSide(uint8_t en, uint8_t inA, uint8_t inB, int s) {
  digitalWrite(inA, s > 0 ? HIGH : LOW);
  digitalWrite(inB, s < 0 ? HIGH : LOW);
  analogWrite(en, abs(s));
}

void initMotors() {
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  stopMotors();
}

void setMotors(int l, int r) {
  speedL = constrain(l, -MAX_SPEED, MAX_SPEED);
  speedR = constrain(r, -MAX_SPEED, MAX_SPEED);
  lastCmd = millis();

  setSide(ENA_PIN, IN1_PIN, IN2_PIN, speedL);
  // prawy silnik zamontowany odwrotnie - zamienione piny kierunku
  setSide(ENB_PIN, IN4_PIN, IN3_PIN, speedR);
}

void stopMotors() {
  setMotors(0, 0);
}

void updateMotors(unsigned long now) {
  if ((speedL != 0 || speedR != 0) && now - lastCmd > MOTOR_TIMEOUT) stopMotors();
}
