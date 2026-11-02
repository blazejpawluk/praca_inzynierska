// Błażej Pawluk

#ifndef WHEELS
#define WHEELS

#include <Arduino.h>

class Wheels {
  public:
    Wheels(uint8_t ENA, uint8_t ENB, uint8_t IN1, uint8_t IN2, uint8_t IN3, uint8_t IN4);

    void setSpeedL(uint8_t);
    void setSpeedR(uint8_t);
    void setSpeed(uint8_t);

    void forwardL();
    void forwardR();
    void forward();

    void backL();
    void backR();
    void back();

    void stopL();
    void stopR();
    void stop();
  
  private:
    uint8_t pinsL[3];
    uint8_t pinsR[3];
    uint8_t speedR, speedL;
};

#endif