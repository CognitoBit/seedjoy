/**
 * SeedJoy - USB HID Controller
 * 
 * Implements USB HID gamepad/joystick interface
 */

#ifndef USB_HID_H
#define USB_HID_H

#include "config.h"
#include <Adafruit_TinyUSB.h>

#if ENABLE_FFB
class FfbRuntime;   // fwd decl; full include in usb_hid.cpp
#endif

class USBHIDController {
public:
  USBHIDController();

  // Initialize USB HID (call in setup)
  bool begin(const DeviceConfig* config);

#if ENABLE_FFB
  // Provide the FFB runtime that receives PID output/feature reports. Call
  // before begin(). Without it, PID reports are ignored (safe no-op).
  void setFfbRuntime(FfbRuntime* rt);
#endif
  
  // Send HID report with current axis and button states
  void sendReport(const int16_t axes[MAX_AXES], const uint8_t buttonBytes[8]);
  
  // Check if USB is connected and ready
  bool isReady();
  
  // Process configuration requests from host
  void processConfigCommands();
  
  // Set callback for config write requests
  void setConfigWriteCallback(void (*callback)(const DeviceConfig*));
  void setConfigReadCallback(DeviceConfig* (*callback)());
  
private:
  Adafruit_USBD_HID usb_hid_;
  
  // HID Report Descriptor for 4-axis, 16-button gamepad
  static const uint8_t HID_REPORT_DESCRIPTOR[];
  static const uint16_t HID_REPORT_DESCRIPTOR_SIZE;
  
  // HID Report structure (4 axes + 64 buttons = 16 bytes)
  struct __attribute__((packed)) HIDReport {
    int16_t x;            // Axis 0
    int16_t y;            // Axis 1
    int16_t z;            // Axis 2
    int16_t rz;           // Axis 3
    uint8_t buttons[8];   // 64 buttons (bit 0-63)
  };
  
  HIDReport report_;
  
  // Configuration callbacks
  void (*configWriteCallback_)(const DeviceConfig*);
  DeviceConfig* (*configReadCallback_)();
  
  // Process incoming data (for configuration)
  static void hidReportCallback(uint8_t report_id, hid_report_type_t report_type,
                                 uint8_t const* buffer, uint16_t bufsize);

#if ENABLE_FFB
  // PID report routing. TinyUSB delivers OUT-endpoint reports with report_id=0
  // and the id as buffer[0]; feature reports carry report_id directly.
  static FfbRuntime* ffbRuntime_;
  static void ffbSetReportCb(uint8_t report_id, hid_report_type_t report_type,
                             uint8_t const* buffer, uint16_t bufsize);
  static uint16_t ffbGetReportCb(uint8_t report_id, hid_report_type_t report_type,
                                 uint8_t* buffer, uint16_t reqlen);
#endif
};

#endif // USB_HID_H
