/**
 * SeedJoy - BLE HID Controller Implementation
 */

#include "ble_hid.h"

// HID Report Map (same as USB HID descriptor)
const uint8_t BLEHIDController::HID_REPORT_MAP[] = {
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x04,        // Usage (Joystick)
  0xA1, 0x01,        // Collection (Application)
  
  // 4 Axes
  0x05, 0x01,        //   Usage Page (Generic Desktop)
  0x09, 0x30,        //   Usage (X)
  0x09, 0x31,        //   Usage (Y)
  0x09, 0x32,        //   Usage (Z)
  0x09, 0x35,        //   Usage (Rz)
  0x16, 0x01, 0x80,  //   Logical Minimum (-32767)
  0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
  0x75, 0x10,        //   Report Size (16)
  0x95, 0x04,        //   Report Count (4)
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  
  // 16 Buttons
  0x05, 0x09,        //   Usage Page (Button)
  0x19, 0x01,        //   Usage Minimum (1)
  0x29, 0x10,        //   Usage Maximum (16)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x10,        //   Report Count (16)
  0x81, 0x02,        //   Input (Data, Variable, Absolute)
  
  0xC0               // End Collection
};

const uint16_t BLEHIDController::HID_REPORT_MAP_SIZE = sizeof(HID_REPORT_MAP);

BLEHIDController::BLEHIDController() : hidEnabled_(false) {
  memset(&report_, 0, sizeof(report_));
}

bool BLEHIDController::begin(const DeviceConfig* config) {
  // Configure Bluefruit
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  
  // Begin Bluefruit
  if (!Bluefruit.begin()) {
    return false;
  }
  
  // Set device name
  Bluefruit.setName(config->deviceName);
  
  // Set TX power
  Bluefruit.setTxPower(config->bleTxPower);
  
  // Set connection callbacks
  Bluefruit.Periph.setConnectCallback(connectCallback);
  Bluefruit.Periph.setDisconnectCallback(disconnectCallback);
  
  // Configure and start Device Information Service
  bledis_.setManufacturer("SeedJoy");
  bledis_.setModel("XIAO nRF52840");
  char fwVersion[16];
  snprintf(fwVersion, sizeof(fwVersion), "%d.%d.%d", 
           FIRMWARE_VERSION_MAJOR, FIRMWARE_VERSION_MINOR, FIRMWARE_VERSION_PATCH);
  bledis_.setFirmwareRev(fwVersion);
  bledis_.begin();
  
  // Start BLE HID service
  blehid_.begin();
  
  // Set HID Report Map
  blehid_.setHidInfo(0x0111, 0x00, 0x01); // HID v1.11, Non-localized, not remote wake
  blehid_.setReportMap(HID_REPORT_MAP, HID_REPORT_MAP_SIZE);
  
  // Start Battery Service
  blebas_.begin();
  blebas_.write(100); // Initial battery level
  
  // Set connection interval
  // min = config->bleConnInterval * 1.25ms
  // max = min * 2 for flexibility
  uint16_t minInterval = config->bleConnInterval;
  uint16_t maxInterval = minInterval * 2;
  if (maxInterval > 100) maxInterval = 100; // Cap at 125ms
  
  Bluefruit.Periph.setConnInterval(minInterval, maxInterval);
  
  // Start advertising
  startAdvertising();
  
  return true;
}

void BLEHIDController::startAdvertising() {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_JOYSTICK);
  
  // Include HID service
  Bluefruit.Advertising.addService(blehid_);
  
  // Include name
  Bluefruit.Advertising.addName();
  
  // Scan response (additional data)
  Bluefruit.ScanResponse.addName();
  
  // Set advertising interval (fast: 20ms, slow: 152.5ms)
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244); // in units of 0.625ms
  Bluefruit.Advertising.setFastTimeout(30);   // Fast mode for 30 seconds
  
  // Start advertising
  Bluefruit.Advertising.start(0); // 0 = Don't stop advertising
}

void BLEHIDController::sendReport(const int16_t axes[MAX_AXES], uint16_t buttonBitmask) {
  if (!isConnected() || !hidEnabled_) return;
  
  // Populate report
  report_.x = axes[0];
  report_.y = axes[1];
  report_.z = axes[2];
  report_.rz = axes[3];
  report_.buttons = buttonBitmask;
  
  // Send input report (reportID = 0)
  blehid_.inputReport(0, (uint8_t*)&report_, sizeof(report_));
}

bool BLEHIDController::isConnected() {
  return Bluefruit.connected();
}

uint16_t BLEHIDController::getConnectionHandle() {
  return Bluefruit.connHandle();
}

void BLEHIDController::setConnectionInterval(uint16_t min, uint16_t max) {
  if (isConnected()) {
    Bluefruit.Periph.setConnInterval(min, max);
  }
}

void BLEHIDController::disconnect() {
  if (isConnected()) {
    Bluefruit.disconnect(getConnectionHandle());
  }
}

void BLEHIDController::connectCallback(uint16_t conn_handle) {
  Serial.println("BLE Connected");
  
  // Get connection info
  BLEConnection* conn = Bluefruit.Connection(conn_handle);
  
  char peer_name[32] = { 0 };
  conn->getPeerName(peer_name, sizeof(peer_name));
  
  Serial.print("Connected to: ");
  Serial.println(peer_name);
  
  // Update connection parameters if needed
  // conn->requestConnectionParameter(min_interval, max_interval);
}

void BLEHIDController::disconnectCallback(uint16_t conn_handle, uint8_t reason) {
  Serial.print("BLE Disconnected, reason: 0x");
  Serial.println(reason, HEX);
}

void BLEHIDController::setHIDEnabled(bool enabled) {
  hidEnabled_ = enabled;
  
  if (enabled) {
    Serial.println("HID reporting ENABLED");
  } else {
    Serial.println("HID reporting DISABLED (configuration mode)");
    
    // Send a zero report to clear any active inputs
    if (isConnected()) {
      memset(&report_, 0, sizeof(report_));
      blehid_.inputReport(0, (uint8_t*)&report_, sizeof(report_));
    }
  }
}

bool BLEHIDController::isHIDEnabled() const {
  return hidEnabled_;
}
