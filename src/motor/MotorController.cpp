#include "MotorController.h"

void MotorController::setup() {
    pinMode(PIN_MOTOR_L_IN1, OUTPUT);
    pinMode(PIN_MOTOR_L_IN2, OUTPUT);
    pinMode(PIN_MOTOR_R_IN1, OUTPUT);
    pinMode(PIN_MOTOR_R_IN2, OUTPUT);

    // L298N ENA/ENB speed channels (5kHz, 8-bit resolution)
    ledcAttach(PIN_MOTOR_L_EN, 5000, 8);
    ledcAttach(PIN_MOTOR_R_EN, 5000, 8);

    stopAll();
    Serial.println("Motors initialized");
}

void MotorController::setMotor(uint8_t in1Pin, uint8_t in2Pin, uint8_t enPin, int speed) {
    speed = constrain(speed, -100, 100);
    if (speed == 0) {
        digitalWrite(in1Pin, LOW);
        digitalWrite(in2Pin, LOW);
        ledcWrite(enPin, 0);
    } else {
        bool forward = speed > 0;
        digitalWrite(in1Pin, forward ? HIGH : LOW);
        digitalWrite(in2Pin, forward ? LOW : HIGH);
        ledcWrite(enPin, (uint32_t)(abs(speed) * 255 / 100));
    }
}

void MotorController::setDrive(int8_t lx, int8_t ly) {
    // Differential drive mixing (tank steering from a single stick).
    // The forward term is negated: pushing the stick forward (ly > 0) has to
    // drive the motors in what setMotor() calls reverse, because the drive
    // motors are mounted facing opposite ways. Steering (lx) is already
    // correct, so it keeps its sign.
    int left  = constrain(-(int)ly + (int)lx, -100, 100);
    int right = constrain(-(int)ly - (int)lx, -100, 100);

    // STATUS_MOVEMENT always carries the left stick, so this runs every
    // ~100ms even during head-only (right-stick) control. Skip the redundant
    // pin writes when we're already stopped and asked to stop again.
    if (left == 0 && right == 0 && !active_) return;

    setMotor(PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, PIN_MOTOR_L_EN, left);
    setMotor(PIN_MOTOR_R_IN1, PIN_MOTOR_R_IN2, PIN_MOTOR_R_EN, right);

    active_ = (left != 0 || right != 0);
    lastCommandTime_ = millis();
}

void MotorController::stopAll() {
    setMotor(PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, PIN_MOTOR_L_EN, 0);
    setMotor(PIN_MOTOR_R_IN1, PIN_MOTOR_R_IN2, PIN_MOTOR_R_EN, 0);
    active_ = false;
}

void MotorController::checkTimeout() {
    if (active_ && (millis() - lastCommandTime_ >= MOTOR_TIMEOUT_MS)) {
        Serial.println("Motor timeout - stopping");
        stopAll();
    }
}
