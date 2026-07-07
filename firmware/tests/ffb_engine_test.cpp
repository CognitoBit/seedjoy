/**
 * Host-side unit test for the FFB effect engine (pure math, no device deps).
 *
 * Build & run:
 *   c++ -std=c++11 -Wall -I../seedjoy ffb_engine_test.cpp ../seedjoy/ffb_engine.cpp -o /tmp/ffbtest && /tmp/ffbtest
 *
 * This is the verification the FFB descriptor/handshake can't get without
 * hardware: the effect math is exercised end-to-end and checked numerically.
 */
#include "ffb_engine.h"

#include <cstdio>
#include <cmath>

static int g_fail = 0;
static int g_pass = 0;

static void check(bool cond, const char* name) {
  if (cond) { g_pass++; }
  else      { g_fail++; printf("  FAIL: %s\n", name); }
}
static void approx(float got, float want, float tol, const char* name) {
  bool ok = fabsf(got - want) <= tol;
  if (ok) { g_pass++; }
  else    { g_fail++; printf("  FAIL: %s (got %.4f, want %.4f +/- %.3f)\n",
                             name, got, want, tol); }
}

// Helper: fully-enabled engine with one effect of `type`, actuators on.
static uint8_t setup(FfbEngine& e, uint8_t type, uint32_t duration = 0) {
  e.reset();
  e.setActuatorsEnabled(true);
  e.setDeviceGain(1.0f);
  uint8_t idx = e.createEffect(type);
  e.setEffectCommon(idx, type, duration, 1.0f, 0x01, nullptr);
  return idx;
}

static void test_pool() {
  printf("pool management\n");
  FfbEngine e; e.reset();
  check(e.poolFree() == MAX_FFB_EFFECTS, "pool starts empty");
  uint8_t first = e.createEffect(FFB_ET_CONSTANT);
  check(first == 1, "first block is index 1");
  for (int i = 1; i < MAX_FFB_EFFECTS; i++) e.createEffect(FFB_ET_CONSTANT);
  check(e.poolFree() == 0, "pool fills");
  check(e.createEffect(FFB_ET_CONSTANT) == 0, "full pool returns 0");
  e.freeEffect(first);
  check(e.poolFree() == 1, "free returns a slot");
  check(e.createEffect(FFB_ET_SPRING) == first, "freed slot is reused");
}

static void test_constant_and_safety() {
  printf("constant force + safety gates\n");
  FfbEngine e;
  uint8_t idx = setup(e, FFB_ET_CONSTANT);
  e.setConstantForce(idx, 0.5f);
  e.startEffect(idx, 1, 0);
  float pos[1] = {0.0f}, f[1] = {0.0f};

  e.update(0, pos, f, 1);
  approx(f[0], 0.5f, 1e-4f, "constant outputs its magnitude");

  e.setActuatorsEnabled(false);
  e.update(1, pos, f, 1);
  approx(f[0], 0.0f, 1e-6f, "actuators disabled => zero force");

  e.setActuatorsEnabled(true);
  e.setPaused(true);
  e.update(2, pos, f, 1);
  approx(f[0], 0.0f, 1e-6f, "paused => zero force");

  e.setPaused(false);
  e.setDeviceGain(0.5f);
  e.update(3, pos, f, 1);
  approx(f[0], 0.25f, 1e-4f, "device gain scales output");
}

static void test_spring() {
  printf("spring (position-proportional, opposing)\n");
  FfbEngine e;
  uint8_t idx = setup(e, FFB_ET_SPRING);
  FfbConditionParams c = {};
  c.cpOffset = 0.0f; c.posCoeff = 1.0f; c.negCoeff = 1.0f;
  c.posSat = 1.0f; c.negSat = 1.0f; c.deadBand = 0.0f;
  e.setCondition(idx, 0, c);
  e.startEffect(idx, 1, 0);

  float f[1];
  float posR[1] = {0.5f};
  e.update(0, posR, f, 1);
  check(f[0] < 0.0f, "push +0.5 => restoring force is negative");
  approx(f[0], -0.5f, 1e-4f, "spring force proportional to displacement");

  float posL[1] = {-0.5f};
  e.update(1, posL, f, 1);
  check(f[0] > 0.0f, "push -0.5 => restoring force is positive");
  approx(f[0], 0.5f, 1e-4f, "spring symmetric");

  // Deadband: within band => no force.
  c.deadBand = 0.2f; e.setCondition(idx, 0, c);
  float posSmall[1] = {0.1f};
  e.update(2, posSmall, f, 1);
  approx(f[0], 0.0f, 1e-6f, "inside deadband => zero");
  float posBig[1] = {0.5f};
  e.update(3, posBig, f, 1);
  approx(f[0], -0.3f, 1e-4f, "outside deadband => (disp - deadband)");

  // Saturation clamps magnitude.
  c.deadBand = 0.0f; c.posSat = 0.25f; e.setCondition(idx, 0, c);
  float posFar[1] = {0.9f};
  e.update(4, posFar, f, 1);
  approx(f[0], -0.25f, 1e-4f, "positive saturation clamps");
}

static void test_damper() {
  printf("damper (opposes velocity)\n");
  FfbEngine e;
  uint8_t idx = setup(e, FFB_ET_DAMPER);
  FfbConditionParams c = {};
  c.posCoeff = 0.1f; c.negCoeff = 0.1f; c.posSat = 1.0f; c.negSat = 1.0f;
  e.setCondition(idx, 0, c);
  e.startEffect(idx, 1, 0);

  // Constant +2.0/s velocity: move 0.002 per 1ms tick.
  float f[1] = {0.0f};
  float p = -0.08f;
  for (uint32_t t = 0; t <= 80; t++) {
    float pos[1] = { p };
    e.update(t, pos, f, 1);
    p += 0.002f;
  }
  check(f[0] < 0.0f, "positive velocity => negative (opposing) force");
  approx(f[0], -0.2f, 0.03f, "damper force ~= coeff * velocity");
}

static void test_sine_frequency() {
  printf("sine periodic (frequency)\n");
  FfbEngine e;
  uint8_t idx = setup(e, FFB_ET_SINE);
  e.setPeriodic(idx, 1.0f, 0.0f, 0.0f, 100 /*ms => 10 Hz*/);
  e.startEffect(idx, 1, 0);

  float pos[1] = {0.0f}, f[1];
  int crossings = 0;
  int lastSign = 0;   // track last *non-zero* sign; exact 0.0 at phase wraps is not a crossing
  for (uint32_t t = 0; t < 1000; t++) {
    e.update(t, pos, f, 1);
    int s = (f[0] > 1e-6f) ? 1 : (f[0] < -1e-6f ? -1 : 0);
    if (s != 0) {
      if (lastSign != 0 && s != lastSign) crossings++;
      lastSign = s;
    }
  }
  // 10 Hz over 1 s => 10 cycles => 20 sign changes.
  check(crossings >= 18 && crossings <= 22, "sine 10 Hz => ~20 zero crossings");
}

static void test_duration_timeout() {
  printf("duration timeout\n");
  FfbEngine e;
  uint8_t idx = setup(e, FFB_ET_CONSTANT, 100 /*ms*/);
  e.setConstantForce(idx, 0.7f);
  e.startEffect(idx, 1, 0);
  float pos[1] = {0.0f}, f[1];

  e.update(50, pos, f, 1);
  approx(f[0], 0.7f, 1e-4f, "force present before timeout");
  e.update(150, pos, f, 1);
  approx(f[0], 0.0f, 1e-6f, "force gone after timeout");
  check(e.playingCount() == 0, "effect auto-stops at duration");
}

static void test_mixing() {
  printf("effect mixing + clamp\n");
  FfbEngine e; e.reset();
  e.setActuatorsEnabled(true); e.setDeviceGain(1.0f);
  uint8_t a = e.createEffect(FFB_ET_CONSTANT);
  uint8_t b = e.createEffect(FFB_ET_CONSTANT);
  e.setEffectCommon(a, FFB_ET_CONSTANT, 0, 1.0f, 0x01, nullptr);
  e.setEffectCommon(b, FFB_ET_CONSTANT, 0, 1.0f, 0x01, nullptr);
  e.setConstantForce(a, 0.6f);
  e.setConstantForce(b, 0.6f);
  e.startEffect(a, 1, 0);
  e.startEffect(b, 1, 0);
  float pos[1] = {0.0f}, f[1];
  e.update(0, pos, f, 1);
  approx(f[0], 1.0f, 1e-4f, "0.6 + 0.6 clamps to 1.0");
}

static void test_per_axis_conditions() {
  printf("per-axis condition blocks\n");
  FfbEngine e; e.reset();
  e.setActuatorsEnabled(true);
  uint8_t idx = e.createEffect(FFB_ET_SPRING);
  e.setEffectCommon(idx, FFB_ET_SPRING, 0, 1.0f, 0x03 /*axes 0+1*/, nullptr);

  FfbConditionParams c0 = {}; c0.posCoeff = 1.0f; c0.negCoeff = 1.0f; c0.posSat = 1.0f; c0.negSat = 1.0f;
  FfbConditionParams c1 = {}; c1.posCoeff = 0.5f; c1.negCoeff = 0.5f; c1.posSat = 1.0f; c1.negSat = 1.0f;
  e.setCondition(idx, 0, c0);
  e.setCondition(idx, 1, c1);
  e.startEffect(idx, 1, 0);

  float pos[2] = {0.5f, 0.5f}, f[2];
  e.update(0, pos, f, 2);
  approx(f[0], -0.5f, 1e-4f, "axis 0 uses cond[0] (coeff 1.0)");
  approx(f[1], -0.25f, 1e-4f, "axis 1 uses cond[1] (coeff 0.5)");
}

static void test_loop_count() {
  printf("loop count\n");
  FfbEngine e;
  uint8_t idx = setup(e, FFB_ET_CONSTANT, 100 /*ms per iteration*/);
  e.setConstantForce(idx, 0.6f);
  e.startEffect(idx, 3 /*loops*/, 0);
  float pos[1] = {0.0f}, f[1];

  e.update(50, pos, f, 1);
  approx(f[0], 0.6f, 1e-4f, "iteration 1 active");
  e.update(250, pos, f, 1);
  approx(f[0], 0.6f, 1e-4f, "still active within 3 iterations");
  e.update(350, pos, f, 1);
  approx(f[0], 0.0f, 1e-6f, "stops after 3 iterations (300 ms)");
}

static void test_max_output_clamp() {
  printf("global max-output safety clamp\n");
  FfbEngine e;
  uint8_t idx = setup(e, FFB_ET_CONSTANT);
  e.setMaxOutput(0.5f);
  e.setConstantForce(idx, 0.9f);
  e.startEffect(idx, 1, 0);
  float pos[1] = {0.0f}, f[1];
  e.update(0, pos, f, 1);
  approx(f[0], 0.5f, 1e-4f, "positive force clamped to maxOutput");
  e.setConstantForce(idx, -0.9f);
  e.update(1, pos, f, 1);
  approx(f[0], -0.5f, 1e-4f, "negative force clamped to -maxOutput");
}

int main() {
  test_pool();
  test_constant_and_safety();
  test_spring();
  test_damper();
  test_sine_frequency();
  test_duration_timeout();
  test_mixing();
  test_per_axis_conditions();
  test_loop_count();
  test_max_output_clamp();

  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
