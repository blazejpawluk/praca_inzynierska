/*
  Błażej Pawluk
  
  Praca inżynierska: "Samojezdna platforma do skanowania pomieszczeń"
  Kod na mikrokontroler Arduino UNO

  Podłączone komponenty:
    - 4 silniki DC z przekładnią sterowane przez sterownik 2-osiowy L298N
      piny sterujące: ENA=5, IN1=4, IN2=3, ENB=6, IN3=7, IN4=8
    - ESP32-CAM
      połączenie UART: RX=D0, TX=D1 (przez konwerter poziomów logicznych 5V-3.3V)
    - wyświetlacz LCD 16x2 I2C
      połączenie I2C: SDA=A4, SCL=A5
    - enkodery szcelinowe kół (po jednym na przednie koło)
      piny sterujące: lewy=A0, prawy=A1
    - serwomechanizm SG90
      pin sterujący: D9
    - sonar ultradźwiękowy HC-SR04
      piny sterujące: TRIG=A3, ECHO=A2
    - żyroskop z akcelerometrem MPU6050
      połączenie I2C: SDA=SDA, SCL=SCL
*/

#include "comm_out.h"
#include "comm_in.h"
#include "lcd.h"
#include "wheels.h"
#include "encoders.h"
#include "servo.h"
#include "sonar.h"
#include "mpu.h"
#include "position.h"
#include "radar.h"
#include "interrupts.h"

// konfiguracja systemu
const unsigned long BAUD = 38400;
const unsigned long SEND_INTERVAL = 100; // ms
unsigned long lastSend = 0; // ms

uint8_t connectionStatus;
int prevStatus;

void setup() {
  // inicjalizacja LCD
  initLCD();
  printLCD(0, "INITIALIZATION");

  // inicjalizacja komunikacji UART
  Serial.begin(BAUD);

  // inicjalizacja silników
  initMotors();

  // inicjalizacja enkoderów
  initEncoders();

  // inicjalizacja SG90
  initServo();

  // inicjalizacja HC-SR04
  initSonar();

  // włączenie przerwań enkoderów i sonaru
  initInterrupts();

  // inicjalizacja MPU6050
  if (!initMPU()) {
    printLCD(0, "MPU6050 ERROR");
    printLCD(1, "RESET THIS CAR");
    while (true);
  }

  // oczekiwanie na połączenie
  while (connectionStatus == 0) {
    requestConnection();
    readSerial();

    printLCD(0, "WAITING FOR WIFI");
  }
  while (connectionStatus == 1) {
    requestConnection();
    readSerial();

    printLCD(0, "CONNECT ON YOUR");
    printLCD(1, "DEVICE");
  }

  // inicjalizacja pozycji
  initPosition();

  printLCD(0, "CONNECTED");
  printLCD(1, "READY TO WORK");
  prevStatus = 2;

  lastSend = millis();
}

void loop() {
  // odbieranie danych z UART
  readSerial();

  // wyświetlenie ostatniej komendy na LCD (najwyżej co LCD_REFRESH_MS)
  showLastCommand();

  if (connectionStatus == 2) {
    if (prevStatus != 2) {
      initPosition();
      printLCD(0, "CONNECTED");
      printLCD(1, "READY TO WORK");
    }
    prevStatus = connectionStatus;

    // zatrzymanie silników po braku komend z frontendu
    checkCmdTimeout();

    // aktualizacja pozycji (o nową próbkę MPU pytamy dopiero, gdy mogła już powstać)
    if (micros() - prevUs >= MPU_POLL_US && dataReadyMPU()) {
      updatePosition();
    }
    
    // aktualizacja radaru
    updateRadar();

    // odbiór wyniku pomiaru sonaru (radar lub pomiar z cyklu wysyłania)
    unsigned int mm;
    if (pollSonar(mm)) {
      queueDist(mm);
      if (radarWaiting) radarNextStep();
    }

    // rozpoczęcie cyklu wysyłania co SEND_INTERVAL ms
    unsigned long now = millis(); // ms
    if (now - lastSend >= SEND_INTERVAL) {
      startSendCycle();
      lastSend = now;
    }

    // wysłanie najwyżej jednej wiadomości (bez blokowania)
    sendNext();
  } else {
    if (prevStatus == 2) {
      setMotors(0,0);
      
      lastCmdPending = false;
      printLCD(0, "DISCONNECTED");
      printLCD(1, "WAITING");
      
      // porzucenie trwającego pomiaru, aby radar nie czekał na niego po ponownym połączeniu
      sonarBusy = false;
      radarWaiting = false;
      resetSendCycle();
    }

    prevStatus = connectionStatus;
    requestConnection();
  }
}