/**
 * SeedJoy - Force Feedback types (pure, host-testable)
 *
 * No Arduino / TinyUSB dependencies on purpose: this header + ffb_engine.* form
 * the effect-math core, which is compiled and unit-tested on the host
 * (firmware/tests/ffb_engine_test.cpp). Wire parsing and USB live elsewhere.
 */
#ifndef FFB_TYPES_H
#define FFB_TYPES_H

#include <stdint.h>

// Max independent force axes the engine mixes for. The current joystick exposes
// 4 axes; hardware (Phase 4) will drive a subset. Kept small and static.
#ifndef MAX_FFB_AXES
#define MAX_FFB_AXES 4
#endif

// Device-managed effect pool size. The host asks for blocks via Create New
// Effect and we hand back an index in [1, MAX_FFB_EFFECTS].
#ifndef MAX_FFB_EFFECTS
#define MAX_FFB_EFFECTS 16
#endif

// Effect type values. These deliberately match the USB PID "Effect Type" (ET)
// usage ordinals (1..12) so the report parser maps wire → enum directly.
enum FfbEffectType {
  FFB_ET_NONE          = 0,
  FFB_ET_CONSTANT      = 1,
  FFB_ET_RAMP          = 2,
  FFB_ET_SQUARE        = 3,
  FFB_ET_SINE          = 4,
  FFB_ET_TRIANGLE      = 5,
  FFB_ET_SAWTOOTH_UP   = 6,
  FFB_ET_SAWTOOTH_DOWN = 7,
  FFB_ET_SPRING        = 8,
  FFB_ET_DAMPER        = 9,
  FFB_ET_INERTIA       = 10,
  FFB_ET_FRICTION      = 11,
  FFB_ET_CUSTOM        = 12
};

// Envelope (applies to constant / ramp / periodic effects; not conditions).
struct FfbEnvelope {
  bool     present;
  float    attackLevel;     // 0..1, level at t=0
  float    fadeLevel;       // 0..1, level at end
  uint32_t attackTime_ms;
  uint32_t fadeTime_ms;
};

// Condition parameters (spring/damper/inertia/friction). All normalized.
// The engine treats a positive coefficient as *resistive* (a spring with
// posCoeff>0 centers the axis) — see ffb_engine.cpp for the sign convention.
// POD (kept memset-safe; the engine zero-inits blocks and sets saturation
// defaults explicitly in createEffect).
struct FfbConditionParams {
  float cpOffset;   // -1..1, center point offset
  float posCoeff;   // 0..1 per unit displacement, positive side
  float negCoeff;   // 0..1, negative side
  float posSat;     // 0..1, saturation (max) on positive side
  float negSat;     // 0..1, saturation on negative side
  float deadBand;   // 0..1, half-width around cpOffset with no force
};

// One effect block. Parameters are stored normalized so the engine is unit-
// agnostic; the report layer converts wire values (int16 DirectInput scale)
// into these.
struct FfbEffect {
  bool     allocated;
  bool     playing;
  uint8_t  type;              // FfbEffectType

  uint32_t duration_ms;       // 0 = infinite
  uint32_t startTime_ms;
  uint8_t  loopCount;

  float    gain;              // 0..1, per-effect gain
  uint8_t  axisMask;          // bit i set => effect acts on axis i
  float    dirScale[MAX_FFB_AXES]; // signed direction weight per axis (-1..1)

  // Constant
  float    magnitude;         // -1..1

  // Ramp
  float    rampStart;         // -1..1
  float    rampEnd;           // -1..1

  // Periodic
  float    periodicMag;       // 0..1 amplitude
  float    periodicOffset;    // -1..1
  float    phase;             // 0..1 (turns)
  uint32_t period_ms;

  // Condition — one parameter block per axis (PID parameterBlockOffset). Spring/
  // damper/etc. can have distinct coefficients per axis.
  FfbConditionParams cond[MAX_FFB_AXES];

  // Envelope
  FfbEnvelope env;
};

#endif // FFB_TYPES_H
