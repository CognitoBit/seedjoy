/**
 * SeedJoy - Force Feedback effect engine (pure computation)
 */
#include "ffb_config.h"
#if ENABLE_FFB

#include "ffb_engine.h"

#include <math.h>
#include <string.h>

// Velocity / acceleration IIR smoothing (0 = no history, 1 = frozen).
static const float VEL_ALPHA = 0.6f;
static const float ACC_ALPHA = 0.7f;

// Largest sane tick used for a derivative. Bigger gaps (first tick, a stall)
// produce a meaningless derivative, so we skip it rather than spike the force.
static const float MAX_DT_S = 0.05f;

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

FfbEngine::FfbEngine() {
  reset();
}

void FfbEngine::reset() {
  memset(effects_, 0, sizeof(effects_));
  deviceGain_ = 1.0f;
  actuatorsEnabled_ = false;
  paused_ = false;
  lastUpdateMs_ = 0;
  haveLast_ = false;
  for (int a = 0; a < MAX_FFB_AXES; a++) {
    lastPos_[a] = 0.0f;
    vel_[a] = 0.0f;
    accel_[a] = 0.0f;
  }
}

bool FfbEngine::indexValid(uint8_t index) const {
  return index >= 1 && index <= MAX_FFB_EFFECTS && effects_[index - 1].allocated;
}

uint8_t FfbEngine::createEffect(uint8_t effectType) {
  for (uint8_t i = 0; i < MAX_FFB_EFFECTS; i++) {
    if (!effects_[i].allocated) {
      FfbEffect& e = effects_[i];
      memset(&e, 0, sizeof(e));
      e.allocated = true;
      e.playing = false;
      e.type = effectType;
      e.gain = 1.0f;
      e.axisMask = 0x01;          // default: axis 0
      e.dirScale[0] = 1.0f;
      return i + 1;               // 1-based
    }
  }
  return 0;                        // pool full
}

void FfbEngine::freeEffect(uint8_t index) {
  if (index >= 1 && index <= MAX_FFB_EFFECTS) {
    memset(&effects_[index - 1], 0, sizeof(FfbEffect));
  }
}

void FfbEngine::freeAll() {
  memset(effects_, 0, sizeof(effects_));
}

uint8_t FfbEngine::poolFree() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_FFB_EFFECTS; i++) {
    if (!effects_[i].allocated) n++;
  }
  return n;
}

void FfbEngine::setEffectCommon(uint8_t index, uint8_t type, uint32_t duration_ms,
                                float gain, uint8_t axisMask, const float* dirScale) {
  if (!indexValid(index)) return;
  FfbEffect& e = effects_[index - 1];
  e.type = type;
  e.duration_ms = duration_ms;
  e.gain = clampf(gain, 0.0f, 1.0f);
  e.axisMask = axisMask ? axisMask : 0x01;
  for (int a = 0; a < MAX_FFB_AXES; a++) {
    if (dirScale) {
      e.dirScale[a] = clampf(dirScale[a], -1.0f, 1.0f);
    } else {
      // Default direction: +1 on every enabled axis.
      e.dirScale[a] = (e.axisMask & (1 << a)) ? 1.0f : 0.0f;
    }
  }
}

void FfbEngine::setEnvelope(uint8_t index, const FfbEnvelope& env) {
  if (!indexValid(index)) return;
  effects_[index - 1].env = env;
}

void FfbEngine::setCondition(uint8_t index, const FfbConditionParams& cond) {
  if (!indexValid(index)) return;
  effects_[index - 1].cond = cond;
}

void FfbEngine::setPeriodic(uint8_t index, float magnitude, float offset,
                            float phase, uint32_t period_ms) {
  if (!indexValid(index)) return;
  FfbEffect& e = effects_[index - 1];
  e.periodicMag = clampf(magnitude, 0.0f, 1.0f);
  e.periodicOffset = clampf(offset, -1.0f, 1.0f);
  e.phase = phase - floorf(phase);
  e.period_ms = period_ms;
}

void FfbEngine::setConstantForce(uint8_t index, float magnitude) {
  if (!indexValid(index)) return;
  effects_[index - 1].magnitude = clampf(magnitude, -1.0f, 1.0f);
}

void FfbEngine::setRampForce(uint8_t index, float start, float end) {
  if (!indexValid(index)) return;
  effects_[index - 1].rampStart = clampf(start, -1.0f, 1.0f);
  effects_[index - 1].rampEnd = clampf(end, -1.0f, 1.0f);
}

void FfbEngine::startEffect(uint8_t index, uint8_t loopCount, uint32_t now_ms) {
  if (!indexValid(index)) return;
  FfbEffect& e = effects_[index - 1];
  e.playing = true;
  e.startTime_ms = now_ms;
  e.loopCount = loopCount;
}

void FfbEngine::stopEffect(uint8_t index) {
  if (!indexValid(index)) return;
  effects_[index - 1].playing = false;
}

void FfbEngine::stopAll() {
  for (uint8_t i = 0; i < MAX_FFB_EFFECTS; i++) {
    effects_[i].playing = false;
  }
}

void FfbEngine::setDeviceGain(float gain)        { deviceGain_ = clampf(gain, 0.0f, 1.0f); }
void FfbEngine::setActuatorsEnabled(bool enabled) { actuatorsEnabled_ = enabled; }
void FfbEngine::setPaused(bool paused)            { paused_ = paused; }

uint8_t FfbEngine::playingCount() const {
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_FFB_EFFECTS; i++) {
    if (effects_[i].allocated && effects_[i].playing) n++;
  }
  return n;
}

const FfbEffect& FfbEngine::effect(uint8_t index) const {
  static FfbEffect empty;
  if (index < 1 || index > MAX_FFB_EFFECTS) return empty;
  return effects_[index - 1];
}

// Envelope multiplier (attack ramp up, fade ramp down). 1.0 when no envelope.
float FfbEngine::envelopeScale(const FfbEffect& e, uint32_t now_ms) const {
  if (!e.env.present) return 1.0f;
  uint32_t t = now_ms - e.startTime_ms;
  float scale = 1.0f;

  if (e.env.attackTime_ms > 0 && t < e.env.attackTime_ms) {
    float frac = (float)t / (float)e.env.attackTime_ms;   // 0..1
    scale = e.env.attackLevel + (1.0f - e.env.attackLevel) * frac;
  }
  if (e.duration_ms > 0 && e.env.fadeTime_ms > 0) {
    uint32_t fadeStart = (e.duration_ms > e.env.fadeTime_ms)
                         ? (e.duration_ms - e.env.fadeTime_ms) : 0;
    if (t > fadeStart) {
      float remain = (e.duration_ms > t) ? (float)(e.duration_ms - t) : 0.0f;
      float frac = remain / (float)e.env.fadeTime_ms;     // 1..0
      float fadeScale = e.env.fadeLevel + (1.0f - e.env.fadeLevel) * frac;
      if (fadeScale < scale) scale = fadeScale;
    }
  }
  return clampf(scale, 0.0f, 1.0f);
}

// DirectInput/PID condition force from a metric (position/velocity/accel).
// Returns the *raw* formula value: same sign as displacement. Callers negate
// it so a positive coefficient becomes a resistive (restoring) force.
float FfbEngine::conditionForce(const FfbConditionParams& c, float metric) const {
  float d = metric - c.cpOffset;
  float f = 0.0f;
  if (d > c.deadBand) {
    f = c.posCoeff * (d - c.deadBand);
    if (f > c.posSat) f = c.posSat;
  } else if (d < -c.deadBand) {
    f = c.negCoeff * (d + c.deadBand);      // d+deadBand < 0 => f < 0
    if (f < -c.negSat) f = -c.negSat;
  }
  return f;
}

float FfbEngine::periodicValue(uint8_t type, float phase) const {
  switch (type) {
    case FFB_ET_SQUARE:        return phase < 0.5f ? 1.0f : -1.0f;
    case FFB_ET_SINE:          return sinf(2.0f * (float)M_PI * phase);
    case FFB_ET_TRIANGLE:      return phase < 0.5f ? (4.0f * phase - 1.0f)
                                                   : (3.0f - 4.0f * phase);
    case FFB_ET_SAWTOOTH_UP:   return 2.0f * phase - 1.0f;
    case FFB_ET_SAWTOOTH_DOWN: return 1.0f - 2.0f * phase;
    default:                   return 0.0f;
  }
}

bool FfbEngine::update(uint32_t now_ms, const float* pos, float* forceOut,
                       uint8_t numAxes) {
  if (numAxes > MAX_FFB_AXES) numAxes = MAX_FFB_AXES;

  // --- Derive velocity and acceleration (filtered) ---
  float dt = 0.0f;
  if (haveLast_ && now_ms > lastUpdateMs_) {
    dt = (now_ms - lastUpdateMs_) / 1000.0f;
  }
  if (dt > 0.0f && dt < MAX_DT_S) {
    for (uint8_t a = 0; a < numAxes; a++) {
      float rawVel = (pos[a] - lastPos_[a]) / dt;
      float newVel = VEL_ALPHA * vel_[a] + (1.0f - VEL_ALPHA) * rawVel;
      float rawAcc = (newVel - vel_[a]) / dt;
      vel_[a] = newVel;
      accel_[a] = ACC_ALPHA * accel_[a] + (1.0f - ACC_ALPHA) * rawAcc;
    }
  }
  for (uint8_t a = 0; a < numAxes; a++) lastPos_[a] = pos[a];
  lastUpdateMs_ = now_ms;
  haveLast_ = true;

  // Zero output first.
  for (uint8_t a = 0; a < numAxes; a++) forceOut[a] = 0.0f;

  // Safety gates: no force unless actuators are on and not paused.
  if (!actuatorsEnabled_ || paused_) return false;

  // --- Mix effects ---
  for (uint8_t i = 0; i < MAX_FFB_EFFECTS; i++) {
    FfbEffect& e = effects_[i];
    if (!e.allocated || !e.playing) continue;

    // Duration timeout (0 = infinite).
    if (e.duration_ms > 0 && (now_ms - e.startTime_ms) >= e.duration_ms) {
      e.playing = false;
      continue;
    }

    float env = envelopeScale(e, now_ms);

    for (uint8_t a = 0; a < numAxes; a++) {
      if (!(e.axisMask & (1 << a))) continue;
      float contrib = 0.0f;

      switch (e.type) {
        case FFB_ET_CONSTANT:
          contrib = e.magnitude * env * e.gain * e.dirScale[a];
          break;

        case FFB_ET_RAMP: {
          float frac = (e.duration_ms > 0)
                       ? (float)(now_ms - e.startTime_ms) / (float)e.duration_ms
                       : 0.0f;
          frac = clampf(frac, 0.0f, 1.0f);
          float v = e.rampStart + (e.rampEnd - e.rampStart) * frac;
          contrib = v * env * e.gain * e.dirScale[a];
          break;
        }

        case FFB_ET_SQUARE:
        case FFB_ET_SINE:
        case FFB_ET_TRIANGLE:
        case FFB_ET_SAWTOOTH_UP:
        case FFB_ET_SAWTOOTH_DOWN: {
          float ph = e.phase + ((e.period_ms > 0)
                     ? (float)(now_ms - e.startTime_ms) / (float)e.period_ms
                     : 0.0f);
          ph -= floorf(ph);
          float v = e.periodicOffset + e.periodicMag * periodicValue(e.type, ph);
          contrib = clampf(v, -1.0f, 1.0f) * env * e.gain * e.dirScale[a];
          break;
        }

        // Condition effects: negate the raw formula so a positive coefficient
        // resists motion (spring centers, damper/friction/inertia oppose).
        case FFB_ET_SPRING:
          contrib = -conditionForce(e.cond, pos[a]) * e.gain * e.dirScale[a];
          break;
        case FFB_ET_DAMPER:
        case FFB_ET_FRICTION:
          contrib = -conditionForce(e.cond, vel_[a]) * e.gain * e.dirScale[a];
          break;
        case FFB_ET_INERTIA:
          contrib = -conditionForce(e.cond, accel_[a]) * e.gain * e.dirScale[a];
          break;

        default:
          break;
      }
      forceOut[a] += contrib;
    }
  }

  // Device gain + final clamp.
  bool active = false;
  for (uint8_t a = 0; a < numAxes; a++) {
    forceOut[a] = clampf(forceOut[a] * deviceGain_, -1.0f, 1.0f);
    if (forceOut[a] != 0.0f) active = true;
  }
  return active;
}

#endif // ENABLE_FFB
