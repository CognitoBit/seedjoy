/**
 * Host-side test for the FFB runtime: exercises the PID report path end to end
 * through raw wire bytes (Create New Effect -> Block Load -> Pool -> Set Effect
 * -> Set Constant -> Device Control -> Effect Operation -> force out -> Free).
 *
 * Build & run:
 *   c++ -std=c++11 -Wall -I../seedjoy ffb_runtime_test.cpp \
 *       ../seedjoy/ffb_runtime.cpp ../seedjoy/ffb_engine.cpp -o /tmp/ffbrt && /tmp/ffbrt
 */
#include "ffb_runtime.h"

#include <cstdio>
#include <cmath>
#include <cstring>

static int g_fail = 0, g_pass = 0;
static void check(bool c, const char* n) {
  if (c) g_pass++; else { g_fail++; printf("  FAIL: %s\n", n); }
}
static void approx(float got, float want, float tol, const char* n) {
  if (fabsf(got - want) <= tol) g_pass++;
  else { g_fail++; printf("  FAIL: %s (got %.4f want %.4f)\n", n, got, want); }
}

template <typename T>
static void out(FfbRuntime& rt, uint8_t id, const T& s, uint32_t now) {
  rt.handleOutputReport(id, reinterpret_cast<const uint8_t*>(&s), sizeof(T), now);
}

int main() {
  FfbRuntime rt;

  printf("create new effect + block load handshake\n");
  uint8_t create[3] = { (uint8_t)FFB_ET_CONSTANT, 0, 0 };
  rt.handleSetFeature(PID_RID_CREATE_EFFECT, create, sizeof(create));
  check(rt.lastCreatedBlock() == 1, "first Create returns block 1");
  check(rt.lastLoadStatus() == PID_LOAD_SUCCESS, "load status success");

  uint8_t buf[16];
  uint16_t n = rt.handleGetFeature(PID_RID_BLOCK_LOAD, buf, sizeof(buf));
  PidBlockLoad bl; memcpy(&bl, buf, sizeof(bl));
  check(n == sizeof(PidBlockLoad), "block load report length");
  check(bl.effectBlockIndex == 1 && bl.loadStatus == PID_LOAD_SUCCESS, "block load payload");

  n = rt.handleGetFeature(PID_RID_POOL, buf, sizeof(buf));
  PidPool pool; memcpy(&pool, buf, sizeof(pool));
  check(n == sizeof(PidPool), "pool report length");
  check(pool.maxSimultaneousEffects == MAX_FFB_EFFECTS, "pool advertises effect count");
  check((pool.memoryManagement & 0x01) != 0, "pool is device-managed");

  printf("full output-report path -> force\n");
  PidSetEffect se; memset(&se, 0, sizeof(se));
  se.effectBlockIndex = 1; se.effectType = FFB_ET_CONSTANT;
  se.duration = PID_DURATION_INFINITE; se.gain = 255; se.enableAxes = 0x01;
  out(rt, PID_RID_SET_EFFECT, se, 0);

  PidSetConstant sc; memset(&sc, 0, sizeof(sc));
  sc.effectBlockIndex = 1; sc.magnitude = 5000;   // 0.5 normalized
  out(rt, PID_RID_SET_CONSTANT, sc, 0);

  // Before enabling actuators, force must stay zero (safety gate).
  float pos[1] = {0.0f}, f[1];
  PidEffectOperation op; memset(&op, 0, sizeof(op));
  op.effectBlockIndex = 1; op.operation = PID_OP_START; op.loopCount = 1;
  out(rt, PID_RID_EFFECT_OP, op, 5);
  rt.engine().update(5, pos, f, 1);
  approx(f[0], 0.0f, 1e-6f, "no force until actuators enabled");

  PidDeviceControl dc; memset(&dc, 0, sizeof(dc));
  dc.control = PID_DC_ENABLE_ACTUATORS;
  out(rt, PID_RID_DEVICE_CONTROL, dc, 6);
  rt.engine().update(6, pos, f, 1);
  approx(f[0], 0.5f, 2e-3f, "constant force flows through full PID path");

  printf("device gain report\n");
  PidDeviceGain dg; memset(&dg, 0, sizeof(dg));
  dg.gain = 128;   // ~0.5
  out(rt, PID_RID_DEVICE_GAIN, dg, 7);
  rt.engine().update(7, pos, f, 1);
  approx(f[0], 0.25f, 5e-3f, "device gain 128/255 halves output");

  printf("PID state report reflects enable\n");
  uint8_t sbuf[2];
  rt.buildStateReport(sbuf, sizeof(sbuf));
  PidState st; memcpy(&st, sbuf, sizeof(st));
  check((st.status & 0x02) != 0, "state: actuators-enabled bit set");

  printf("direction sign (single axis)\n");
  // Re-point the same block: direction 128 = 180deg => dir = cos(pi) = -1, so a
  // positive constant magnitude should now produce negative force.
  se.directionX = 128;
  out(rt, PID_RID_SET_EFFECT, se, 10);
  dg.gain = 255; out(rt, PID_RID_DEVICE_GAIN, dg, 10);   // restore full gain
  rt.engine().update(11, pos, f, 1);
  approx(f[0], -0.5f, 5e-3f, "direction 180deg flips constant force sign");
  se.directionX = 0;
  out(rt, PID_RID_SET_EFFECT, se, 12);
  rt.engine().update(13, pos, f, 1);
  approx(f[0], 0.5f, 5e-3f, "direction 0deg keeps positive sign");

  printf("pause gate + block free\n");
  dc.control = PID_DC_PAUSE; out(rt, PID_RID_DEVICE_CONTROL, dc, 8);
  rt.engine().update(8, pos, f, 1);
  approx(f[0], 0.0f, 1e-6f, "pause zeroes force");
  dc.control = PID_DC_CONTINUE; out(rt, PID_RID_DEVICE_CONTROL, dc, 9);

  PidBlockFree bf; memset(&bf, 0, sizeof(bf));
  bf.effectBlockIndex = 1; out(rt, PID_RID_BLOCK_FREE, bf, 10);
  check(rt.engine().poolFree() == MAX_FFB_EFFECTS, "block freed back to pool");

  printf("pool exhaustion\n");
  int created = 0;
  for (int i = 0; i < MAX_FFB_EFFECTS + 3; i++) {
    uint8_t c2[3] = { (uint8_t)FFB_ET_SPRING, 0, 0 };
    rt.handleSetFeature(PID_RID_CREATE_EFFECT, c2, sizeof(c2));
    if (rt.lastCreatedBlock() != 0) created++;
    else check(rt.lastLoadStatus() == PID_LOAD_FULL, "full pool reports LOAD_FULL");
  }
  check(created == MAX_FFB_EFFECTS, "exactly pool-size effects allocatable");

  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
