#pragma once
#include <Arduino.h>
#include "../config.h"

// Differential-drive controller for a dual-channel L298N H-bridge (two DC
// gear motors, one per side).
class MotorController {
public:
    void setup();
    void setDrive(int8_t lx, int8_t ly);
    void stopAll();
    void checkTimeout();

private:
    void setMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t enPin, int speed);

    unsigned long lastCommandTime_ = 0;
    bool active_ = false;
};
