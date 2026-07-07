/**
 * SeedJoy - USB HID Controller Implementation
 */

#include "usb_hid.h"

#if ENABLE_FFB
#include "ffb_reports.h"
#include "ffb_runtime.h"
FfbRuntime* USBHIDController::ffbRuntime_ = nullptr;
#endif

// HID Report Descriptor for gamepad with 4 axes and 16 buttons
const uint8_t USBHIDController::HID_REPORT_DESCRIPTOR[] = {
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x04,        // Usage (Joystick)
  0xA1, 0x01,        // Collection (Application)
  
  // 4 Axes (X, Y, Z, Rz)
  0x05, 0x01,        //   Usage Page (Generic Desktop)
  0x09, 0x30,        //   Usage (X)
  0x09, 0x31,        //   Usage (Y)
  0x09, 0x32,        //   Usage (Z)
  0x09, 0x35,        //   Usage (Rz)
  0x16, 0x01, 0x80,  //   Logical Minimum (-32767)
  0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
  0x75, 0x10,        //   Report Size (16 bits)
  0x95, 0x04,        //   Report Count (4 axes)
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  
  // 64 Buttons (8 bytes; SR-only uses first 56)
  0x05, 0x09,        //   Usage Page (Button)
  0x19, 0x01,        //   Usage Minimum (Button 1)
  0x29, 0x40,        //   Usage Maximum (Button 64)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1 bit)
  0x95, 0x40,        //   Report Count (64 buttons)
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  
  0xC0               // End Collection
};

const uint16_t USBHIDController::HID_REPORT_DESCRIPTOR_SIZE = sizeof(HID_REPORT_DESCRIPTOR);

USBHIDController::USBHIDController() :
#if ENABLE_FFB
  // FFB: PID descriptor (joystick is Report ID 1) + interrupt OUT endpoint for
  // host->device output reports.
  usb_hid_(FFB_HID_REPORT_DESCRIPTOR, FFB_HID_REPORT_DESCRIPTOR_SIZE, HID_ITF_PROTOCOL_NONE, 1, true),
#else
  usb_hid_(HID_REPORT_DESCRIPTOR, HID_REPORT_DESCRIPTOR_SIZE, HID_ITF_PROTOCOL_NONE, 1, false),
#endif
  configWriteCallback_(nullptr),
  configReadCallback_(nullptr) {

  memset(&report_, 0, sizeof(report_));
}

#if ENABLE_FFB
void USBHIDController::setFfbRuntime(FfbRuntime* rt) {
  ffbRuntime_ = rt;
}
#endif

bool USBHIDController::begin(const DeviceConfig* config) {
  // Set device descriptor
  TinyUSBDevice.setManufacturerDescriptor("SeedJoy");
  TinyUSBDevice.setProductDescriptor(config->deviceName);

#if ENABLE_FFB
  // Distinct PID for the FFB identity so hosts don't reuse a cached non-FFB
  // descriptor.
  TinyUSBDevice.setID(config->usbVID ? config->usbVID : 0x239A, FFB_USB_PID);
#else
  if (config->usbVID != 0 && config->usbPID != 0) {
    TinyUSBDevice.setID(config->usbVID, config->usbPID);
  }
#endif

  // Initialize USB HID
  usb_hid_.begin();

#if ENABLE_FFB
  // Route PID output/feature reports to the FFB runtime.
  usb_hid_.setReportCallback(ffbGetReportCb, ffbSetReportCb);
#else
  // Set output report callback (for receiving data from host)
  usb_hid_.setReportCallback(nullptr, hidReportCallback);
#endif
  
  // Wait for USB to be ready (with timeout)
  uint32_t start = millis();
  while (!TinyUSBDevice.mounted() && millis() - start < 3000) {
    delay(1);
  }
  
  return TinyUSBDevice.mounted();
}

void USBHIDController::sendReport(const int16_t axes[MAX_AXES], const uint8_t buttonBytes[8]) {
  if (!isReady()) return;
  
  // Populate report structure
  report_.x = axes[0];
  report_.y = axes[1];
  report_.z = axes[2];
  report_.rz = axes[3];
  memcpy(report_.buttons, buttonBytes, 8);

#if ENABLE_FFB
  // Joystick is Report ID 1 in the PID descriptor.
  usb_hid_.sendReport(PID_RID_JOYSTICK, &report_, sizeof(report_));
#else
  // Send report (report ID 0)
  usb_hid_.sendReport(0, &report_, sizeof(report_));
#endif
}

bool USBHIDController::isReady() {
  return TinyUSBDevice.mounted() && usb_hid_.ready();
}

void USBHIDController::processConfigCommands() {
  // Configuration is handled via callbacks in hidReportCallback
  // Nothing to do here in polling
}

void USBHIDController::setConfigWriteCallback(void (*callback)(const DeviceConfig*)) {
  configWriteCallback_ = callback;
}

void USBHIDController::setConfigReadCallback(DeviceConfig* (*callback)()) {
  configReadCallback_ = callback;
}

void USBHIDController::hidReportCallback(uint8_t report_id, hid_report_type_t report_type,
                                         uint8_t const* buffer, uint16_t bufsize) {
  // Handle configuration requests (OUTPUT reports)
  // For MVP, we'll implement config via serial or BLE instead
  // Can be extended later to support USB configuration
}

#if ENABLE_FFB
// Host -> device (SET_REPORT / OUT endpoint). Demux the report id and hand off
// to the FFB runtime.
// CONTEXT: invoked from tud_task() on the high-priority "usbd" FreeRTOS task —
// NOT from loop(). It mutates FfbEngine state that updateFFB() reads from the
// loop task; that race is documented in seedjoy.ino/updateFFB and must be
// resolved (report queue) before the FFB path is trusted.
void USBHIDController::ffbSetReportCb(uint8_t report_id, hid_report_type_t report_type,
                                      uint8_t const* buffer, uint16_t bufsize) {
  if (!ffbRuntime_) return;
  uint32_t now = millis();

  if (report_type == HID_REPORT_TYPE_OUTPUT) {
    // On the interrupt OUT endpoint TinyUSB passes report_id=0 with the real id
    // as the first payload byte; on a control SET_REPORT the id is provided.
    if (report_id == 0 && bufsize > 0) {
      report_id = buffer[0];
      buffer++;
      bufsize--;
    }
    ffbRuntime_->handleOutputReport(report_id, buffer, bufsize, now);
  } else if (report_type == HID_REPORT_TYPE_FEATURE) {
    ffbRuntime_->handleSetFeature(report_id, buffer, bufsize);
  }
}

// Device -> host (GET_REPORT). Feature reports = Block Load / Pool; the PID
// State input report can also be fetched by id.
uint16_t USBHIDController::ffbGetReportCb(uint8_t report_id, hid_report_type_t report_type,
                                         uint8_t* buffer, uint16_t reqlen) {
  if (!ffbRuntime_) return 0;
  if (report_type == HID_REPORT_TYPE_FEATURE) {
    return ffbRuntime_->handleGetFeature(report_id, buffer, reqlen);
  }
  if (report_type == HID_REPORT_TYPE_INPUT && report_id == PID_RID_STATE) {
    return ffbRuntime_->buildStateReport(buffer, reqlen);
  }
  return 0;
}
#endif // ENABLE_FFB
