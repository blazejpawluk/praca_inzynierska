// Błażej Pawluk

#include "Wheels.h"

// orange, blue, yellow, green, brown, purple
Wheels w(3, 6, 4, 5, 7, 8);

void setup() {
  w.forwardR();
  delay(1000);
  w.stopR();
  delay(300);
  w.forwardL();
  delay(1000);
  w.stopL();
  delay(300);
  w.forward();
  delay(2000);
  w.stop();
}

void loop() {
}
