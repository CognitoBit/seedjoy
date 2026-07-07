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
  
  // Start advertising (pass config service to include in advertising packet)
  void startAdvertising(BLEService* configService = nullptr);
  
  // Send HID report with current axis and button states
  void sendReport(const int16_t axes[MAX_AXES], const uint8_t buttonBytes[8]);
  
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
  
public:
  // Update BLE battery service level (0-100%)
  void setBatteryLevel(uint8_t percent);

private:
  BLEDis bledis_;          // Device Information Service
  BLEHidGeneric blehid_;   // HID Service (custom joystick report map)
  BLEBas blebas_;          // Battery Service
  
  bool hidEnabled_;       // Flag to enable/disable HID reports
  
  // HID Report Map (same as USB)
  static const uint8_t HID_REPORT_MAP[];
  static const uint16_t HID_REPORT_MAP_SIZE;
  
  // HID Report structure (4 axes + 64 buttons = 16 bytes)
  struct __attribute__((packed)) HIDReport {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t rz;
    uint8_t buttons[8];   // 64 buttons (bit 0-63)
  };
  
  HIDReport report_;
  
  // Connection callbacks
  static void connectCallback(uint16_t conn_handle);
  static void disconnectCallback(uint16_t conn_handle, uint8_t reason);
};

#endif // BLE_HID_H
