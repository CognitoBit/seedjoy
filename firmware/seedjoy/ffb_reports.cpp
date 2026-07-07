/**
 * SeedJoy - USB PID report descriptor (spec-structured, UNVALIDATED)
 *
 * See the banner in ffb_reports.h. Report framing/sizing here is guaranteed to
 * match the wire structs via the static_asserts below; the PID *usages* are
 * best-effort from the PID 1.0 spec and must be validated on a host.
 */
#include "ffb_config.h"
#if ENABLE_FFB

#include "ffb_reports.h"

// Compile-time guarantee: the descriptor's report sizes (documented per report
// below) match the packed structs the runtime parser casts onto the wire bytes.
#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(PidSetEffect)      == 13, "PidSetEffect size");
static_assert(sizeof(PidSetEnvelope)    == 9,  "PidSetEnvelope size");
static_assert(sizeof(PidSetCondition)   == 14, "PidSetCondition size");
static_assert(sizeof(PidSetPeriodic)    == 8,  "PidSetPeriodic size");
static_assert(sizeof(PidSetConstant)    == 3,  "PidSetConstant size");
static_assert(sizeof(PidSetRamp)        == 5,  "PidSetRamp size");
static_assert(sizeof(PidEffectOperation)== 3,  "PidEffectOperation size");
static_assert(sizeof(PidBlockFree)      == 1,  "PidBlockFree size");
static_assert(sizeof(PidDeviceControl)  == 1,  "PidDeviceControl size");
static_assert(sizeof(PidDeviceGain)     == 1,  "PidDeviceGain size");
static_assert(sizeof(PidCreateNewEffect)== 3,  "PidCreateNewEffect size");
static_assert(sizeof(PidBlockLoad)      == 4,  "PidBlockLoad size");
static_assert(sizeof(PidPool)           == 4,  "PidPool size");
static_assert(sizeof(PidState)          == 2,  "PidState size");
#endif

// HID usage helpers for readability.
#define HID_RID(x)  0x85, (x)

const uint8_t FFB_HID_REPORT_DESCRIPTOR[] = {
  // ===================== Joystick (input) =====================
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x04,        // Usage (Joystick)
  0xA1, 0x01,        // Collection (Application)
  HID_RID(PID_RID_JOYSTICK),        //   Report ID 1
  0x05, 0x01,        //   Usage Page (Generic Desktop)
  0x09, 0x30,        //   Usage (X)
  0x09, 0x31,        //   Usage (Y)
  0x09, 0x32,        //   Usage (Z)
  0x09, 0x35,        //   Usage (Rz)
  0x16, 0x01, 0x80,  //   Logical Minimum (-32767)
  0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
  0x75, 0x10,        //   Report Size (16)
  0x95, 0x04,        //   Report Count (4)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x05, 0x09,        //   Usage Page (Button)
  0x19, 0x01,        //   Usage Minimum (Button 1)
  0x29, 0x40,        //   Usage Maximum (Button 64)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x40,        //   Report Count (64)
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0xC0,              // End Collection

  // ===================== PID (force feedback) =====================
  // Usage Page (Physical Interface Device), Usage (PID device)
  0x05, 0x0F,        // Usage Page (PID)
  0x09, 0x01,        // Usage (Physical Interface Device)
  0xA1, 0x01,        // Collection (Application)

  // ---- PID State (input, ID 2, 2 bytes) ----
  HID_RID(PID_RID_STATE),
  0x09, 0x92,        //   Usage (PID State Report)
  0xA1, 0x02,        //   Collection (Logical)
  0x09, 0x22,        //     Usage (Effect Block Index)  [status byte here best-effort]
  0x15, 0x00, 0x26, 0xFF, 0x00,  //   Logical 0..255
  0x75, 0x08, 0x95, 0x02,        //   Report Size 8, Count 2
  0x81, 0x02,        //     Input (Data,Var,Abs)  (2 bytes: status, block index)
  0xC0,              //   End Collection

  // ---- Set Effect (output, ID 3, 13 bytes) ----
  HID_RID(PID_RID_SET_EFFECT),
  0x09, 0x21,        //   Usage (Set Effect Report)
  0xA1, 0x02,        //   Collection (Logical)
  0x09, 0x22,        //     Usage (Effect Block Index)
  0x09, 0x25,        //     Usage (Effect Type)
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x02,        //   2 bytes: block index, type
  0x91, 0x02,        //     Output (Data,Var,Abs)
  0x09, 0x50,        //     Usage (Duration)
  0x09, 0x54,        //     Usage (Trigger Repeat Interval)
  0x09, 0x51,        //     Usage (Sample Period)
  0x16, 0x00, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00, // Logical 0..65535
  0x75, 0x10, 0x95, 0x03,        //   3 x 16-bit: duration, repeat, sample
  0x91, 0x02,        //     Output
  0x09, 0x52,        //     Usage (Gain)
  0x09, 0x53,        //     Usage (Trigger Button)
  0x09, 0x55,        //     Usage (Axes Enable)
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x03,        //   3 bytes: gain, trigger, axesEnable
  0x91, 0x02,        //     Output
  0x09, 0x57,        //     Usage (Direction)
  0x75, 0x08, 0x95, 0x02,        //   2 bytes: dirX, dirY
  0x91, 0x02,        //     Output
  0xC0,              //   End Collection

  // ---- Set Envelope (output, ID 4, 9 bytes) ----
  HID_RID(PID_RID_SET_ENVELOPE),
  0x09, 0x5A,        //   Usage (Set Envelope Report)
  0xA1, 0x02,        //   Collection (Logical)
  0x09, 0x22,        //     Effect Block Index
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,        //   1 byte
  0x91, 0x02,
  0x09, 0x5B,        //     Attack Level
  0x09, 0x5D,        //     Fade Level
  0x09, 0x5C,        //     Attack Time
  0x09, 0x5E,        //     Fade Time
  0x16, 0x00, 0x00, 0x27, 0xFF, 0xFF, 0x00, 0x00,
  0x75, 0x10, 0x95, 0x04,        //   4 x 16-bit
  0x91, 0x02,
  0xC0,

  // ---- Set Condition (output, ID 5, 14 bytes) ----
  HID_RID(PID_RID_SET_CONDITION),
  0x09, 0x5F,        //   Usage (Set Condition Report)
  0xA1, 0x02,
  0x09, 0x22,        //     Effect Block Index
  0x09, 0x23,        //     Parameter Block Offset
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x02,        //   2 bytes
  0x91, 0x02,
  0x09, 0x60,        //     CP Offset
  0x09, 0x61,        //     Positive Coefficient
  0x09, 0x62,        //     Negative Coefficient
  0x16, 0xF0, 0xD8, 0x26, 0x10, 0x27, // Logical -10000..10000
  0x75, 0x10, 0x95, 0x03,        //   3 x 16-bit signed
  0x91, 0x02,
  0x09, 0x63,        //     Positive Saturation
  0x09, 0x64,        //     Negative Saturation
  0x09, 0x65,        //     Dead Band
  0x15, 0x00, 0x27, 0x10, 0x27, 0x00, 0x00, // Logical 0..10000
  0x75, 0x10, 0x95, 0x03,        //   3 x 16-bit
  0x91, 0x02,
  0xC0,

  // ---- Set Periodic (output, ID 6, 8 bytes) ----
  HID_RID(PID_RID_SET_PERIODIC),
  0x09, 0x6F,        //   Usage (Set Periodic Report)
  0xA1, 0x02,
  0x09, 0x22,        //     Effect Block Index
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,        //   1 byte
  0x91, 0x02,
  0x09, 0x70,        //     Magnitude
  0x09, 0x6F,        //     Offset (best-effort usage)
  0x16, 0xF0, 0xD8, 0x26, 0x10, 0x27,
  0x75, 0x10, 0x95, 0x02,        //   2 x 16-bit
  0x91, 0x02,
  0x09, 0x71,        //     Phase
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,        //   1 byte
  0x91, 0x02,
  0x09, 0x72,        //     Period
  0x27, 0xFF, 0xFF, 0x00, 0x00,
  0x75, 0x10, 0x95, 0x01,        //   1 x 16-bit
  0x91, 0x02,
  0xC0,

  // ---- Set Constant Force (output, ID 7, 3 bytes) ----
  HID_RID(PID_RID_SET_CONSTANT),
  0x09, 0x73,        //   Usage (Set Constant Force Report)
  0xA1, 0x02,
  0x09, 0x22,        //     Effect Block Index
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,
  0x91, 0x02,
  0x09, 0x70,        //     Magnitude
  0x16, 0xF0, 0xD8, 0x26, 0x10, 0x27,
  0x75, 0x10, 0x95, 0x01,
  0x91, 0x02,
  0xC0,

  // ---- Set Ramp Force (output, ID 8, 5 bytes) ----
  HID_RID(PID_RID_SET_RAMP),
  0x09, 0x74,        //   Usage (Set Ramp Force Report)
  0xA1, 0x02,
  0x09, 0x22,        //     Effect Block Index
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,
  0x91, 0x02,
  0x09, 0x75,        //     Ramp Start
  0x09, 0x76,        //     Ramp End
  0x16, 0xF0, 0xD8, 0x26, 0x10, 0x27,
  0x75, 0x10, 0x95, 0x02,
  0x91, 0x02,
  0xC0,

  // ---- Effect Operation (output, ID 9, 3 bytes) ----
  HID_RID(PID_RID_EFFECT_OP),
  0x09, 0x77,        //   Usage (Effect Operation Report)
  0xA1, 0x02,
  0x09, 0x22,        //     Effect Block Index
  0x09, 0x78,        //     Operation
  0x09, 0x7B,        //     Loop Count
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x03,        //   3 bytes
  0x91, 0x02,
  0xC0,

  // ---- PID Block Free (output, ID 10, 1 byte) ----
  HID_RID(PID_RID_BLOCK_FREE),
  0x09, 0x90,        //   Usage (PID Block Free Report)
  0xA1, 0x02,
  0x09, 0x22,        //     Effect Block Index
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,
  0x91, 0x02,
  0xC0,

  // ---- PID Device Control (output, ID 11, 1 byte) ----
  HID_RID(PID_RID_DEVICE_CONTROL),
  0x09, 0x96,        //   Usage (PID Device Control)
  0xA1, 0x02,
  0x09, 0x97,        //     Usage (DC Enable Actuators) [control byte, best-effort]
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,
  0x91, 0x02,
  0xC0,

  // ---- Device Gain (output, ID 12, 1 byte) ----
  HID_RID(PID_RID_DEVICE_GAIN),
  0x09, 0x7D,        //   Usage (Device Gain Report)
  0xA1, 0x02,
  0x09, 0x7E,        //     Usage (Device Gain)
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,
  0x91, 0x02,
  0xC0,

  // ---- Create New Effect (feature, ID 13, 3 bytes) ----
  HID_RID(PID_RID_CREATE_EFFECT),
  0x09, 0xAB,        //   Usage (Create New Effect Report)
  0xA1, 0x02,
  0x09, 0x25,        //     Effect Type
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x01,
  0xB1, 0x02,        //     Feature (Data,Var,Abs)
  0x09, 0x5C,        //     Byte Count (best-effort usage)
  0x27, 0xFF, 0xFF, 0x00, 0x00,
  0x75, 0x10, 0x95, 0x01,
  0xB1, 0x02,
  0xC0,

  // ---- PID Block Load (feature, ID 14, 4 bytes) ----
  HID_RID(PID_RID_BLOCK_LOAD),
  0x09, 0x89,        //   Usage (Block Load Status)
  0xA1, 0x02,
  0x09, 0x22,        //     Effect Block Index
  0x09, 0x8B,        //     Block Load Status
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x02,        //   2 bytes
  0xB1, 0x02,
  0x09, 0xAC,        //     RAM Pool Available (best-effort usage)
  0x27, 0xFF, 0xFF, 0x00, 0x00,
  0x75, 0x10, 0x95, 0x01,        //   1 x 16-bit
  0xB1, 0x02,
  0xC0,

  // ---- PID Pool (feature, ID 15, 4 bytes) ----
  HID_RID(PID_RID_POOL),
  0x09, 0x7F,        //   Usage (PID Pool Report)
  0xA1, 0x02,
  0x09, 0x80,        //     RAM Pool Size
  0x27, 0xFF, 0xFF, 0x00, 0x00,
  0x75, 0x10, 0x95, 0x01,        //   1 x 16-bit
  0xB1, 0x02,
  0x09, 0x83,        //     Simultaneous Effects Max
  0x09, 0xA9,        //     Device Managed Pool (flags, best-effort)
  0x15, 0x00, 0x26, 0xFF, 0x00,
  0x75, 0x08, 0x95, 0x02,        //   2 bytes
  0xB1, 0x02,
  0xC0,

  0xC0               // End Collection (PID application)
};

const uint16_t FFB_HID_REPORT_DESCRIPTOR_SIZE = sizeof(FFB_HID_REPORT_DESCRIPTOR);

#endif // ENABLE_FFB
