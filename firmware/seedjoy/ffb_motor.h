/**
 * SeedJoy - Force Feedback motor output (Phase 4)
 *
 * Maps the engine's per-axis force (-1..1) to physical actuators. Supports the
 * two common hobby motor-driver topologies; NO pins are assigned by default
 * (the XIAO's GPIOs are all allocated — see docs/ffb-plan.md for the D4/D5/D10
 * repurposing options). Until a driver is configured in begin(), axis-0 |force|
 * is mirrored to the status LED so the engine can be exercised with no motor.
 *
 * Not host-tested (Arduino/PWM dependent). PWM carrier frequency is left at the
 * core default here; raising it to ~20 kHz (inaudible) is a hardware-bring-up
 * detail once a driver/motor is chosen — deliberately not hard-coded.
 */
#ifndef FFB_MOTOR_H
#define FFB_MOTOR_H

#include "ffb_config.h"
#if ENABLE_FFB

#include "ffb_types.h"
#include <stdint.h>

enum MotorDriverType {
  MOTOR_DRIVER_NONE = 0,   // no motor wired (LED visualization only)
  MOTOR_DRIVER_PWM_DIR,    // PWM magnitude + DIR sign (TB6612 / A4950 style)
  MOTOR_DRIVER_DUAL_PWM    // IN1/IN2 (DRV8871): fwd = PWM/0, rev = 0/PWM
};

struct MotorAxisConfig {
  MotorDriverType type;    // default NONE
  uint8_t pinA;            // PWM_DIR: PWM pin;  DUAL_PWM: IN1
  uint8_t pinB;            // PWM_DIR: DIR pin;  DUAL_PWM: IN2
  bool    invert;          // flip force sign to match motor wiring polarity
};

class MotorOutput {
public:
  MotorOutput();

  void begin();

  // f in -1..1. Drives the configured driver for `axis` and mirrors axis 0 to
  // the status LED for visualization.
  void setForce(uint8_t axis, float f);

  // Emergency stop — zero every output immediately (USB detach/suspend, PID
  // "stop all", watchdog). Never leaves a motor energized.
  void disableAll();

  bool hasMotorPins() const;

private:
  MotorAxisConfig axis_[MAX_FFB_AXES];
  bool begun_;

  void driveAxis(uint8_t axis, float f);
  void driveLedViz(float f0);
};

#endif // ENABLE_FFB
#endif // FFB_MOTOR_H
