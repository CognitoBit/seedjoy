/**
 * SeedJoy - Force Feedback motor output (Phase 4 scaffold)
 *
 * Abstraction between the effect engine's per-axis force and physical actuators.
 * NO motor pins are assigned by default (the XIAO's GPIOs are all allocated —
 * see docs/ffb-plan.md Phase 4 for the D4/D5/D10 repurposing options). Until a
 * driver is wired, setForce() mirrors axis-0 |force| to the status LED so the
 * engine can be exercised on the bench with no motor (the plan's "LED PWM"
 * visualization for Phase 3 acceptance).
 */
#ifndef FFB_MOTOR_H
#define FFB_MOTOR_H

#include "ffb_config.h"
#if ENABLE_FFB

#include "ffb_types.h"
#include <stdint.h>

class MotorOutput {
public:
  MotorOutput();

  void begin();

  // f in -1..1. Drives configured motor pins (sign-magnitude: DIR = sign,
  // PWM = |f|) and mirrors axis 0 to the status LED for visualization.
  void setForce(uint8_t axis, float f);

  // Emergency stop — zero every output immediately (USB detach/suspend, PID
  // "stop all", watchdog). Never leaves a motor energized.
  void disableAll();

  // True once real driver pins have been assigned for any axis.
  bool hasMotorPins() const;

private:
  // Per-axis driver pins. 0xFF = unconfigured. Assign in begin() (Phase 4).
  uint8_t pwmPin_[MAX_FFB_AXES];
  uint8_t dirPin_[MAX_FFB_AXES];
  bool    begun_;

  void driveLedViz(float f0);
};

#endif // ENABLE_FFB
#endif // FFB_MOTOR_H
