/**
 * SeedJoy - Force Feedback runtime (report parsing + PID handshake)
 */
#include "ffb_config.h"
#if ENABLE_FFB

#include "ffb_runtime.h"

#include <string.h>
#include <math.h>

static inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
// int16 magnitude/coefficient (-10000..10000) -> normalized -1..1
static inline float normS(int16_t v) { return clampf(v / PID_SCALE_MAG, -1.0f, 1.0f); }
// uint16 level/saturation (0..10000) -> normalized 0..1
static inline float normU(uint16_t v) { return clampf(v / PID_SCALE_MAG, 0.0f, 1.0f); }

FfbRuntime::FfbRuntime()
  : pendingBlockIndex_(0), pendingLoadStatus_(PID_LOAD_ERROR) {}

void FfbRuntime::handleOutputReport(uint8_t reportId, const uint8_t* data,
                                    uint16_t len, uint32_t now_ms) {
  switch (reportId) {
    case PID_RID_SET_EFFECT: {
      if (len < sizeof(PidSetEffect)) return;
      PidSetEffect r; memcpy(&r, data, sizeof(r));
      uint32_t dur = (r.duration == PID_DURATION_INFINITE) ? 0 : r.duration;
      float gain = r.gain / PID_SCALE_GAIN;
      uint8_t mask = r.enableAxes ? r.enableAxes : 0x01;

      // Direction -> per-axis signed weight. directionX is a polar angle
      // (0..255 => 0..360deg): the first enabled axis gets cos(theta), the
      // second sin(theta). For a single axis this yields the correct sign
      // (0deg=+1, 180deg=-1). Only non-condition effects use these weights (the
      // engine ignores direction for conditions). The polar mapping and the
      // overall motor sign are a physical-rig tuning item — see docs/ffb-plan.md.
      float theta = (r.directionX / 256.0f) * 6.28318530718f;
      float dir[MAX_FFB_AXES];
      int seen = 0;
      for (int a = 0; a < MAX_FFB_AXES; a++) {
        if (mask & (1 << a)) {
          dir[a] = (seen == 0) ? cosf(theta) : (seen == 1) ? sinf(theta) : 1.0f;
          seen++;
        } else {
          dir[a] = 0.0f;
        }
      }
      engine_.setEffectCommon(r.effectBlockIndex, r.effectType, dur, gain, mask, dir);
      break;
    }
    case PID_RID_SET_ENVELOPE: {
      if (len < sizeof(PidSetEnvelope)) return;
      PidSetEnvelope r; memcpy(&r, data, sizeof(r));
      FfbEnvelope env;
      env.present = true;
      env.attackLevel = normU(r.attackLevel);
      env.fadeLevel = normU(r.fadeLevel);
      env.attackTime_ms = r.attackTime;
      env.fadeTime_ms = r.fadeTime;
      engine_.setEnvelope(r.effectBlockIndex, env);
      break;
    }
    case PID_RID_SET_CONDITION: {
      if (len < sizeof(PidSetCondition)) return;
      PidSetCondition r; memcpy(&r, data, sizeof(r));
      FfbConditionParams c;
      c.cpOffset = normS(r.cpOffset);
      c.posCoeff = normS(r.positiveCoefficient);
      c.negCoeff = normS(r.negativeCoefficient);
      c.posSat = normU(r.positiveSaturation);
      c.negSat = normU(r.negativeSaturation);
      c.deadBand = normU(r.deadBand);
      // parameterBlockOffset selects which axis's condition block this is.
      engine_.setCondition(r.effectBlockIndex, r.parameterBlockOffset, c);
      break;
    }
    case PID_RID_SET_PERIODIC: {
      if (len < sizeof(PidSetPeriodic)) return;
      PidSetPeriodic r; memcpy(&r, data, sizeof(r));
      engine_.setPeriodic(r.effectBlockIndex, normU(r.magnitude), normS(r.offset),
                          r.phase / PID_SCALE_PHASE, r.period);
      break;
    }
    case PID_RID_SET_CONSTANT: {
      if (len < sizeof(PidSetConstant)) return;
      PidSetConstant r; memcpy(&r, data, sizeof(r));
      engine_.setConstantForce(r.effectBlockIndex, normS(r.magnitude));
      break;
    }
    case PID_RID_SET_RAMP: {
      if (len < sizeof(PidSetRamp)) return;
      PidSetRamp r; memcpy(&r, data, sizeof(r));
      engine_.setRampForce(r.effectBlockIndex, normS(r.start), normS(r.end));
      break;
    }
    case PID_RID_EFFECT_OP: {
      if (len < sizeof(PidEffectOperation)) return;
      PidEffectOperation r; memcpy(&r, data, sizeof(r));
      if (r.operation == PID_OP_STOP) {
        engine_.stopEffect(r.effectBlockIndex);
      } else {
        engine_.startEffect(r.effectBlockIndex, r.loopCount, now_ms);
      }
      break;
    }
    case PID_RID_BLOCK_FREE: {
      if (len < sizeof(PidBlockFree)) return;
      PidBlockFree r; memcpy(&r, data, sizeof(r));
      engine_.freeEffect(r.effectBlockIndex);
      break;
    }
    case PID_RID_DEVICE_CONTROL: {
      if (len < sizeof(PidDeviceControl)) return;
      PidDeviceControl r; memcpy(&r, data, sizeof(r));
      switch (r.control) {
        case PID_DC_ENABLE_ACTUATORS:  engine_.setActuatorsEnabled(true);  break;
        case PID_DC_DISABLE_ACTUATORS: engine_.setActuatorsEnabled(false); break;
        case PID_DC_STOP_ALL:          engine_.stopAll();                  break;
        case PID_DC_RESET:             engine_.freeAll(); engine_.stopAll();
                                       engine_.setPaused(false);           break;
        case PID_DC_PAUSE:             engine_.setPaused(true);            break;
        case PID_DC_CONTINUE:          engine_.setPaused(false);           break;
        default: break;
      }
      break;
    }
    case PID_RID_DEVICE_GAIN: {
      if (len < sizeof(PidDeviceGain)) return;
      PidDeviceGain r; memcpy(&r, data, sizeof(r));
      engine_.setDeviceGain(r.gain / PID_SCALE_GAIN);
      break;
    }
    default:
      break;
  }
}

void FfbRuntime::handleSetFeature(uint8_t reportId, const uint8_t* data, uint16_t len) {
  if (reportId != PID_RID_CREATE_EFFECT) return;
  if (len < 1) return;
  uint8_t effectType = data[0];   // PidCreateNewEffect.effectType
  uint8_t idx = engine_.createEffect(effectType);
  pendingBlockIndex_ = idx;
  pendingLoadStatus_ = idx ? PID_LOAD_SUCCESS : PID_LOAD_FULL;
}

uint16_t FfbRuntime::handleGetFeature(uint8_t reportId, uint8_t* buffer, uint16_t reqlen) {
  switch (reportId) {
    case PID_RID_BLOCK_LOAD: {
      if (reqlen < sizeof(PidBlockLoad)) return 0;
      PidBlockLoad r;
      r.effectBlockIndex = pendingBlockIndex_;
      r.loadStatus = pendingLoadStatus_;
      r.ramPoolAvailable = (uint16_t)(engine_.poolFree() * FFB_BLOCK_RAM);
      memcpy(buffer, &r, sizeof(r));
      return sizeof(r);
    }
    case PID_RID_POOL: {
      if (reqlen < sizeof(PidPool)) return 0;
      PidPool r;
      r.ramPoolSize = (uint16_t)(MAX_FFB_EFFECTS * FFB_BLOCK_RAM);
      r.maxSimultaneousEffects = MAX_FFB_EFFECTS;
      r.memoryManagement = 0x01;   // device-managed pool
      memcpy(buffer, &r, sizeof(r));
      return sizeof(r);
    }
    default:
      return 0;
  }
}

uint16_t FfbRuntime::buildStateReport(uint8_t* buffer, uint16_t maxlen) {
  if (maxlen < sizeof(PidState)) return 0;
  uint8_t playing = engine_.firstPlaying();
  PidState s;
  s.status = (uint8_t)((engine_.paused() ? 0x01 : 0x00) |
                       (engine_.actuatorsEnabled() ? 0x02 : 0x00) |
                       (playing ? 0x04 : 0x00));   // bit2 = an effect is playing
  s.effectBlockIndex = playing;                    // 0 = none
  memcpy(buffer, &s, sizeof(s));
  return sizeof(s);
}

#endif // ENABLE_FFB
