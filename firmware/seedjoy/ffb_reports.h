/**
 * SeedJoy - USB PID (force feedback) report definitions
 *
 * Contains:
 *   - Report ID constants (shared by the descriptor and the runtime parser)
 *   - Packed wire structs for each PID report (payload AFTER the report-id byte)
 *   - Normalization scales (wire int -> engine float)
 *   - The USB HID report descriptor with the PID collection
 *
 * ============================================================================
 *  !!  THE DESCRIPTOR BELOW IS SPEC-STRUCTURED BUT **NOT HARDWARE-VALIDATED**  !!
 * ----------------------------------------------------------------------------
 *  It was assembled from the USB "Device Class Definition for Physical
 *  Interface Devices" (PID) 1.0 and the common DIY-FFB lineage. It compiles and
 *  is byte-shaped correctly, but Windows' PID class driver is unforgiving and
 *  the fine-grained usages have not been checked against a real host. Before
 *  trusting it, validate in this order (see docs/ffb-plan.md, Phase 2):
 *    1. Parse with a HID report-descriptor decoder (e.g. hidrd / usbdescgen) —
 *       zero errors, report sizes match the structs in this file.
 *    2. Linux: `fftest /dev/input/eventX` enumerates + uploads every effect.
 *    3. usbmon / Wireshark: Create New Effect -> Block Load handshake is correct.
 *    4. Windows: enumerates under the PID driver; DirectInput `fedit` works.
 *  Report sizes here are the contract the runtime parser relies on; keep the
 *  structs and the descriptor's Report Size/Count in lockstep if you edit them.
 * ============================================================================
 */
#ifndef FFB_REPORTS_H
#define FFB_REPORTS_H

#include <stdint.h>

// ---- Report IDs (must match the descriptor) ----
enum {
  PID_RID_JOYSTICK        = 1,   // input  (axes + buttons)
  PID_RID_STATE           = 2,   // input  (PID State)
  PID_RID_SET_EFFECT      = 3,   // output
  PID_RID_SET_ENVELOPE    = 4,   // output
  PID_RID_SET_CONDITION   = 5,   // output
  PID_RID_SET_PERIODIC    = 6,   // output
  PID_RID_SET_CONSTANT    = 7,   // output
  PID_RID_SET_RAMP        = 8,   // output
  PID_RID_EFFECT_OP       = 9,   // output
  PID_RID_BLOCK_FREE      = 10,  // output
  PID_RID_DEVICE_CONTROL  = 11,  // output
  PID_RID_DEVICE_GAIN     = 12,  // output
  PID_RID_CREATE_EFFECT   = 13,  // feature (host SET, then GET block load)
  PID_RID_BLOCK_LOAD      = 14,  // feature (device -> host)
  PID_RID_POOL            = 15   // feature (device -> host)
};

// ---- Effect Operation values ----
enum { PID_OP_START = 1, PID_OP_START_SOLO = 2, PID_OP_STOP = 3 };

// ---- PID Device Control values ----
enum {
  PID_DC_ENABLE_ACTUATORS  = 1,
  PID_DC_DISABLE_ACTUATORS = 2,
  PID_DC_STOP_ALL          = 3,
  PID_DC_RESET             = 4,
  PID_DC_PAUSE             = 5,
  PID_DC_CONTINUE          = 6
};

// ---- Block Load status ----
enum { PID_LOAD_SUCCESS = 1, PID_LOAD_FULL = 2, PID_LOAD_ERROR = 3 };

// ---- Normalization scales (wire integer -> engine float) ----
// Magnitudes / coefficients / offsets are int16 in DirectInput's -10000..10000.
static const float PID_SCALE_MAG   = 10000.0f;  // divide int16 by this -> -1..1
static const float PID_SCALE_GAIN  = 255.0f;    // uint8 gain -> 0..1
static const float PID_SCALE_PHASE = 256.0f;    // uint8 phase -> turns 0..1
static const uint16_t PID_DURATION_INFINITE = 0xFFFF;

// ---- Wire structs: payload AFTER the report-id byte, little-endian, packed ----
#pragma pack(push, 1)

struct PidSetEffect {          // ID 3  (13 bytes)
  uint8_t  effectBlockIndex;
  uint8_t  effectType;         // 1..12
  uint16_t duration;           // ms, 0xFFFF = infinite
  uint16_t triggerRepeatInterval;
  uint16_t samplePeriod;
  uint8_t  gain;               // 0..255
  uint8_t  triggerButton;
  uint8_t  enableAxes;         // bit0 = X, bit1 = Y (direction-enable bits)
  uint8_t  directionX;         // 0..255 (angle)
  uint8_t  directionY;         // 0..255
};

struct PidSetEnvelope {        // ID 4  (9 bytes)
  uint8_t  effectBlockIndex;
  uint16_t attackLevel;        // 0..10000
  uint16_t fadeLevel;          // 0..10000
  uint16_t attackTime;         // ms
  uint16_t fadeTime;           // ms
};

struct PidSetCondition {       // ID 5  (14 bytes)
  uint8_t  effectBlockIndex;
  uint8_t  parameterBlockOffset;   // 0 = axis 0, 1 = axis 1, ...
  int16_t  cpOffset;               // -10000..10000
  int16_t  positiveCoefficient;
  int16_t  negativeCoefficient;
  uint16_t positiveSaturation;     // 0..10000
  uint16_t negativeSaturation;
  uint16_t deadBand;               // 0..10000
};

struct PidSetPeriodic {        // ID 6  (8 bytes)
  uint8_t  effectBlockIndex;
  uint16_t magnitude;          // 0..10000
  int16_t  offset;             // -10000..10000
  uint8_t  phase;              // 0..255
  uint16_t period;             // ms
};

struct PidSetConstant {        // ID 7  (3 bytes)
  uint8_t  effectBlockIndex;
  int16_t  magnitude;          // -10000..10000
};

struct PidSetRamp {            // ID 8  (5 bytes)
  uint8_t  effectBlockIndex;
  int16_t  start;              // -10000..10000
  int16_t  end;
};

struct PidEffectOperation {    // ID 9  (3 bytes)
  uint8_t  effectBlockIndex;
  uint8_t  operation;          // PID_OP_*
  uint8_t  loopCount;
};

struct PidBlockFree {          // ID 10 (1 byte)
  uint8_t  effectBlockIndex;
};

struct PidDeviceControl {      // ID 11 (1 byte)
  uint8_t  control;            // PID_DC_*
};

struct PidDeviceGain {         // ID 12 (1 byte)
  uint8_t  gain;               // 0..255
};

struct PidCreateNewEffect {    // ID 13 feature in (3 bytes)
  uint8_t  effectType;         // 1..12
  uint16_t byteCount;          // for custom force data
};

struct PidBlockLoad {          // ID 14 feature out (4 bytes)
  uint8_t  effectBlockIndex;
  uint8_t  loadStatus;         // PID_LOAD_*
  uint16_t ramPoolAvailable;
};

struct PidPool {               // ID 15 feature out (4 bytes)
  uint16_t ramPoolSize;
  uint8_t  maxSimultaneousEffects;
  uint8_t  memoryManagement;   // flags: bit0 = device-managed pool
};

struct PidState {              // ID 2 input (2 bytes)
  uint8_t  status;             // bit0 paused, bit1 actuators enabled, bit2 safety switch
  uint8_t  effectBlockIndex;   // playing effect (0 = none)
};

#pragma pack(pop)

// ---- The HID report descriptor (joystick + PID collection) ----
// Declared in ffb_reports.cpp. See the banner at the top of this file.
extern const uint8_t  FFB_HID_REPORT_DESCRIPTOR[];
extern const uint16_t FFB_HID_REPORT_DESCRIPTOR_SIZE;

#endif // FFB_REPORTS_H
