/**
 * SeedJoy - Force Feedback effect engine (pure computation)
 *
 * Owns the effect pool and turns effect parameters + axis motion into a signed
 * force per axis. No I/O, no timers, no USB — the 1 kHz driver (ffb_runtime on
 * device) calls update() each tick; the host test calls it directly.
 *
 * Sign convention (see spring/damper in ffb_engine.cpp): a positive condition
 * coefficient produces a *resistive* force (spring centers, damper opposes
 * motion). Host/motor polarity may still require a global output inversion —
 * that is a Phase 4 hardware-validation knob, not an engine concern.
 */
#ifndef FFB_ENGINE_H
#define FFB_ENGINE_H

#include "ffb_types.h"

class FfbEngine {
public:
  FfbEngine();

  // Wipe all state (free pool, zero motion history, gain=1, actuators off).
  void reset();

  // --- Block management (driven by Create New Effect / Block Free) ---
  // Returns a 1-based block index, or 0 if the pool is full.
  uint8_t createEffect(uint8_t effectType);
  void    freeEffect(uint8_t index);
  void    freeAll();
  uint8_t poolFree() const;      // number of unallocated blocks

  // --- Parameter setters (driven by Set* output reports), index 1-based ---
  void setEffectCommon(uint8_t index, uint8_t type, uint32_t duration_ms,
                       float gain, uint8_t axisMask, const float* dirScale);
  void setEnvelope(uint8_t index, const FfbEnvelope& env);
  void setCondition(uint8_t index, const FfbConditionParams& cond);
  void setPeriodic(uint8_t index, float magnitude, float offset,
                   float phase, uint32_t period_ms);
  void setConstantForce(uint8_t index, float magnitude);
  void setRampForce(uint8_t index, float start, float end);

  // --- Operations (Effect Operation / Device Control) ---
  void startEffect(uint8_t index, uint8_t loopCount, uint32_t now_ms);
  void stopEffect(uint8_t index);
  void stopAll();
  void setDeviceGain(float gain);        // 0..1
  void setActuatorsEnabled(bool enabled);
  void setPaused(bool paused);

  // --- The tick ---
  // now_ms: monotonic ms. pos[axis]: normalized position -1..1 (0 = center).
  // Writes forceOut[axis] in -1..1. Velocity/acceleration are derived
  // internally (filtered). Returns true if any nonzero force is being output.
  bool update(uint32_t now_ms, const float* pos, float* forceOut,
              uint8_t numAxes);

  // --- Introspection (for the PID State input report) ---
  bool    actuatorsEnabled() const { return actuatorsEnabled_; }
  bool    paused() const           { return paused_; }
  uint8_t playingCount() const;

  // Exposed for the host test / diagnostics.
  const FfbEffect& effect(uint8_t index) const;

private:
  FfbEffect effects_[MAX_FFB_EFFECTS];

  float   deviceGain_;
  bool    actuatorsEnabled_;
  bool    paused_;

  // Motion history for velocity/acceleration derivation.
  uint32_t lastUpdateMs_;
  bool     haveLast_;
  float    lastPos_[MAX_FFB_AXES];
  float    vel_[MAX_FFB_AXES];
  float    accel_[MAX_FFB_AXES];

  bool  indexValid(uint8_t index) const;
  float envelopeScale(const FfbEffect& e, uint32_t now_ms) const;
  float conditionForce(const FfbConditionParams& c, float metric) const;
  float periodicValue(uint8_t type, float phase) const;
};

#endif // FFB_ENGINE_H
