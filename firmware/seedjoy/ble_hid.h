/**
 * SeedJoy - BLE HID Controller
 * 
 * Implements Bluetooth Low Energy HID gamepad interface
 */

#ifndef BLE_HID_H
#define BLE_HID_H

#include "config.h"
#include <bluefruit.h>

class BLEHIDController {
public:
  BLEHIDController();
  
  // Initialize BLE HID (call in setup)
  bool begin(const DeviceConfig* config);
  
  // Start advertising
  void startAdvertising();
  
  // Send HID report with current axis and button states
  void sendReport(const int16_t axes[MAX_AXES], uint16_t buttonBitmask);
  
  // Check if BLE is connected
  bool isConnected();
  
  // Get connection handle
  uint16_t getConnectionHandle();
  
  // Set connection interval (in 1.25ms units)
  void setConnectionInterval(uint16_t min, uint16_t max);
  
  // Disconnect
  void disconnect();
  
  // Enable/disable HID reporting (for configuration mode)
  void setHIDEnabled(bool enabled);
  bool isHIDEnabled() const;
  
private:
  BLEDis bledis_;         // Device Information Service
  BLEHidAdafruit blehid_; // HID Service
  BLEBas blebas_;         // Battery Service
  
  bool hidEnabled_;       // Flag to enable/disable HID reports
  
  // HID Report Map (same as USB)
  static const uint8_t HID_REPORT_MAP[];
  static const uint16_t HID_REPORT_MAP_SIZE;
  
  // HID Report structure
  struct __attribute__((packed)) HIDReport {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t rz;
    uint16_t buttons;
  };
  
  HIDReport report_;
  
  // Connection callbacks
  static void connectCallback(uint16_t conn_handle);
  static void disconnectCallback(uint16_t conn_handle, uint8_t reason);
};

#endif // BLE_HID_H
