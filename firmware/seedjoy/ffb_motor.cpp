/**
 * SeedJoy - Force Feedback motor output (Phase 4)
 */
#include "ffb_config.h"
#if ENABLE_FFB

#include "ffb_motor.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

// PWM duty is 8-bit on the Adafruit nRF52 core by default (analogWrite 0..255).
static const int PWM_MAX = 255;

MotorOutput::MotorOutput() : begun_(false) {
  for (int a = 0; a < MAX_FFB_AXES; a++) {
    axis_[a].type = MOTOR_DRIVER_NONE;
    axis_[a].pinA = 0xFF;
    axis_[a].pinB = 0xFF;
    axis_[a].invert = false;
  }
}

void MotorOutput::begin() {
  // ---- Phase 4: assign real driver(s) here. Example (option A, 1 axis) ----
  //   axis_[0].type   = MOTOR_DRIVER_DUAL_PWM;  // DRV8871
  //   axis_[0].pinA   = D4;                      // IN1
  //   axis_[0].pinB   = D5;                      // IN2
  //   axis_[0].invert = false;                   // flip if the spring pushes away
  // Left as NONE on purpose so nothing drives an unknown pin.

  for (int a = 0; a < MAX_FFB_AXES; a++) {
    if (axis_[a].type == MOTOR_DRIVER_NONE) continue;
    if (axis_[a].pinA != 0xFF) { pinMode(axis_[a].pinA, OUTPUT); analogWrite(axis_[a].pinA, 0); }
    if (axis_[a].pinB != 0xFF) {
      pinMode(axis_[a].pinB, OUTPUT);
      if (axis_[a].type == MOTOR_DRIVER_DUAL_PWM) analogWrite(axis_[a].pinB, 0);
      else                                        digitalWrite(axis_[a].pinB, LOW);
    }
  }

  pinMode(STATUS_LED_PIN, OUTPUT);
  driveLedViz(0.0f);
  begun_ = true;
}

bool MotorOutput::hasMotorPins() const {
  for (int a = 0; a < MAX_FFB_AXES; a++) {
    if (axis_[a].type != MOTOR_DRIVER_NONE) return true;
  }
  return false;
}

void MotorOutput::setForce(uint8_t axis, float f) {
  if (!begun_ || axis >= MAX_FFB_AXES) return;
  if (f > 1.0f) f = 1.0f;
  if (f < -1.0f) f = -1.0f;
  if (axis_[axis].invert) f = -f;

  driveAxis(axis, f);

  if (axis == 0) driveLedViz(f);
}

void MotorOutput::driveAxis(uint8_t axis, float f) {
  const MotorAxisConfig& c = axis_[axis];
  if (c.type == MOTOR_DRIVER_NONE) return;

  uint8_t duty = (uint8_t)(fabsf(f) * PWM_MAX + 0.5f);
  bool forward = (f >= 0.0f);

  switch (c.type) {
    case MOTOR_DRIVER_PWM_DIR:
      if (c.pinB != 0xFF) digitalWrite(c.pinB, forward ? HIGH : LOW);
      if (c.pinA != 0xFF) analogWrite(c.pinA, duty);
      break;
    case MOTOR_DRIVER_DUAL_PWM:
      // DRV8871-style: drive one input with PWM, hold the other at 0.
      if (forward) {
        if (c.pinA != 0xFF) analogWrite(c.pinA, duty);
        if (c.pinB != 0xFF) analogWrite(c.pinB, 0);
      } else {
        if (c.pinA != 0xFF) analogWrite(c.pinA, 0);
        if (c.pinB != 0xFF) analogWrite(c.pinB, duty);
      }
      break;
    default:
      break;
  }
}

void MotorOutput::disableAll() {
  for (int a = 0; a < MAX_FFB_AXES; a++) {
    if (axis_[a].type == MOTOR_DRIVER_NONE) continue;
    if (axis_[a].pinA != 0xFF) analogWrite(axis_[a].pinA, 0);
    if (axis_[a].pinB != 0xFF) {
      if (axis_[a].type == MOTOR_DRIVER_DUAL_PWM) analogWrite(axis_[a].pinB, 0);
      else                                        digitalWrite(axis_[a].pinB, LOW);
    }
  }
  if (begun_) driveLedViz(0.0f);
}

// XIAO status LED is active-LOW: analogWrite(255) = off, analogWrite(0) = full.
void MotorOutput::driveLedViz(float f0) {
  uint8_t mag = (uint8_t)(fabsf(f0) * PWM_MAX + 0.5f);
  analogWrite(STATUS_LED_PIN, PWM_MAX - mag);
}

#endif // ENABLE_FFB
