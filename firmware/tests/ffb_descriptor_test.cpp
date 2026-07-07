/**
 * Host-side structural validator for the PID HID report descriptor.
 *
 * Walks FFB_HID_REPORT_DESCRIPTOR as HID items, accumulates each report's bit
 * size per (report id, Input/Output/Feature), and asserts:
 *   - collections balance (every Collection has an End Collection)
 *   - every report's byte size equals the packed wire struct the parser uses
 *
 * This does NOT prove the descriptor is semantically valid to a host (only a
 * real HID stack / fftest can), but it catches descriptor<->struct drift — the
 * most likely way the two get out of sync — without any hardware.
 *
 * Build & run:
 *   c++ -std=c++11 -Wall -DENABLE_FFB=1 -I../seedjoy ffb_descriptor_test.cpp \
 *       ../seedjoy/ffb_reports.cpp -o /tmp/ffbdesc && /tmp/ffbdesc
 */
#include "ffb_reports.h"

#include <cstdio>
#include <cstdint>
#include <map>

static int g_fail = 0, g_pass = 0;
static void check(bool c, const char* n) {
  if (c) g_pass++; else { g_fail++; printf("  FAIL: %s\n", n); }
}

int main() {
  const uint8_t* d = FFB_HID_REPORT_DESCRIPTOR;
  uint16_t len = FFB_HID_REPORT_DESCRIPTOR_SIZE;

  uint32_t reportSize = 0, reportCount = 0;
  int reportId = 0;
  int depth = 0, maxDepth = 0;
  bool balanced = true;

  // bits[type][reportId] where type: 0=Input, 1=Output, 2=Feature
  std::map<int, uint32_t> bits[3];

  uint16_t i = 0;
  while (i < len) {
    uint8_t prefix = d[i++];
    uint8_t bSize = prefix & 0x03;
    uint8_t realSize = (bSize == 3) ? 4 : bSize;
    uint8_t bType = (prefix >> 2) & 0x03;
    uint8_t bTag = (prefix >> 4) & 0x0F;

    uint32_t data = 0;
    for (uint8_t k = 0; k < realSize && i < len; k++) data |= ((uint32_t)d[i++]) << (8 * k);

    if (bType == 1) {          // Global
      if (bTag == 0x7) reportSize = data;        // Report Size
      else if (bTag == 0x9) reportCount = data;  // Report Count
      else if (bTag == 0x8) reportId = (int)data;// Report ID
    } else if (bType == 0) {   // Main
      if (bTag == 0x8) bits[0][reportId] += reportSize * reportCount;       // Input
      else if (bTag == 0x9) bits[1][reportId] += reportSize * reportCount;  // Output
      else if (bTag == 0xB) bits[2][reportId] += reportSize * reportCount;  // Feature
      else if (bTag == 0xA) { depth++; if (depth > maxDepth) maxDepth = depth; } // Collection
      else if (bTag == 0xC) { depth--; if (depth < 0) balanced = false; }   // End Collection
    }
    // Local items (bType==2) ignored — they don't affect report sizing.
  }

  printf("descriptor structure\n");
  check(balanced && depth == 0, "collections balanced (equal Collection/End)");
  check(maxDepth >= 2, "nested logical collections present");

  // Bits must be whole bytes.
  auto bytesOf = [](std::map<int,uint32_t>& m, int id) -> int {
    if (!m.count(id)) return -1;
    uint32_t b = m[id];
    return (b % 8 == 0) ? (int)(b / 8) : -100 - (int)b;   // negative => not byte-aligned
  };

  printf("report sizes match wire structs\n");
  // Input reports
  check(bytesOf(bits[0], PID_RID_JOYSTICK) == 16, "joystick input = 16 bytes");
  check(bytesOf(bits[0], PID_RID_STATE) == (int)sizeof(PidState), "PID state input matches PidState");
  // Output reports
  check(bytesOf(bits[1], PID_RID_SET_EFFECT)     == (int)sizeof(PidSetEffect),      "Set Effect");
  check(bytesOf(bits[1], PID_RID_SET_ENVELOPE)   == (int)sizeof(PidSetEnvelope),    "Set Envelope");
  check(bytesOf(bits[1], PID_RID_SET_CONDITION)  == (int)sizeof(PidSetCondition),   "Set Condition");
  check(bytesOf(bits[1], PID_RID_SET_PERIODIC)   == (int)sizeof(PidSetPeriodic),    "Set Periodic");
  check(bytesOf(bits[1], PID_RID_SET_CONSTANT)   == (int)sizeof(PidSetConstant),    "Set Constant");
  check(bytesOf(bits[1], PID_RID_SET_RAMP)       == (int)sizeof(PidSetRamp),        "Set Ramp");
  check(bytesOf(bits[1], PID_RID_EFFECT_OP)      == (int)sizeof(PidEffectOperation),"Effect Operation");
  check(bytesOf(bits[1], PID_RID_BLOCK_FREE)     == (int)sizeof(PidBlockFree),      "Block Free");
  check(bytesOf(bits[1], PID_RID_DEVICE_CONTROL) == (int)sizeof(PidDeviceControl),  "Device Control");
  check(bytesOf(bits[1], PID_RID_DEVICE_GAIN)    == (int)sizeof(PidDeviceGain),     "Device Gain");
  // Feature reports
  check(bytesOf(bits[2], PID_RID_CREATE_EFFECT)  == (int)sizeof(PidCreateNewEffect),"Create New Effect");
  check(bytesOf(bits[2], PID_RID_BLOCK_LOAD)     == (int)sizeof(PidBlockLoad),      "Block Load");
  check(bytesOf(bits[2], PID_RID_POOL)           == (int)sizeof(PidPool),           "Pool");

  printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail ? 1 : 0;
}
