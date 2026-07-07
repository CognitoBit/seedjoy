/**
 * SeedJoy - Force Feedback motor output (Phase 4 scaffold)
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
    pwmPin_[a] = 0xFF;
    dirPin_[a] = 0xFF;
  }
}

void MotorOutput::begin() {
  // ---- Phase 4: assign real driver pins here. Example (option A, 1 axis) ----
  //   pwmPin_[0] = D4;   // DRV8871 IN1 (PWM magnitude)
  //   dirPin_[0] = D5;   // DRV8871 IN2 / DIR
  // NOTE: DRV8871 has IN1/IN2 (no dedicated DIR); a true sign-magnitude drive
  // needs PWM steered to IN1 or IN2 by sign — refine when the driver is chosen.
  // Left unconfigured on purpose so nothing drives an unknown pin.

  for (int a = 0; a < MAX_FFB_AXES; a++) {
    if (pwmPin_[a] != 0xFF) { pinMode(pwmPin_[a], OUTPUT); analogWrite(pwmPin_[a], 0); }
    if (dirPin_[a] != 0xFF) { pinMode(dirPin_[a], OUTPUT); digitalWrite(dirPin_[a], LOW); }
  }

  // Status LED as the bench visualization output.
  pinMode(STATUS_LED_PIN, OUTPUT);
  driveLedViz(0.0f);
  begun_ = true;
}

bool MotorOutput::hasMotorPins() const {
  for (int a = 0; a < MAX_FFB_AXES; a++) {
    if (pwmPin_[a] != 0xFF) return true;
  }
  return false;
}

void MotorOutput::setForce(uint8_t axis, float f) {
  if (!begun_ || axis >= MAX_FFB_AXES) return;
  if (f > 1.0f) f = 1.0f;
  if (f < -1.0f) f = -1.0f;

  if (pwmPin_[axis] != 0xFF) {
    uint8_t duty = (uint8_t)(fabsf(f) * PWM_MAX + 0.5f);
    if (dirPin_[axis] != 0xFF) digitalWrite(dirPin_[axis], (f >= 0.0f) ? HIGH : LOW);
    analogWrite(pwmPin_[axis], duty);
  }

  // Visualize axis 0 regardless of whether a motor is wired.
  if (axis == 0) driveLedViz(f);
}

void MotorOutput::disableAll() {
  for (int a = 0; a < MAX_FFB_AXES; a++) {
    if (pwmPin_[a] != 0xFF) analogWrite(pwmPin_[a], 0);
  }
  if (begun_) driveLedViz(0.0f);
}

// XIAO status LED is active-LOW: analogWrite(255) = off, analogWrite(0) = full.
void MotorOutput::driveLedViz(float f0) {
  uint8_t mag = (uint8_t)(fabsf(f0) * PWM_MAX + 0.5f);
  analogWrite(STATUS_LED_PIN, PWM_MAX - mag);
}

#endif // ENABLE_FFB
