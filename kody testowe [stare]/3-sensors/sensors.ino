// Błażej Pawluk

#include "Wheels.h"
#include "PinChangeInterrupt.h"

#define SENSOR_LEFT_PIN A0
#define SENSOR_RIGHT_PIN A1

// orange, blue, yellow, green, brown, purple
Wheels w(3, 6, 4, 5, 7, 8);

// sensor counters
volatile int sensorL, sensorR;

void setup() {
  Serial.begin(9600);

  pinMode(SENSOR_LEFT_PIN, INPUT);
  pinMode(SENSOR_RIGHT_PIN, INPUT);

  sensorL = 0; sensorR = 0;
  PCICR = 0x02;
  PCMSK1 = 0x03;

  // w.forward();
}

void loop() {
  Serial.print("left: ");
  Serial.print(sensorL);
  Serial.print(", right: ");
  Serial.println(sensorR);
  delay(250);
}

ISR(PCINT1_vect){
  if (PINC & (1 << PC0)) sensorL++;
  if (PINC & (1 << PC1)) sensorR++;
}