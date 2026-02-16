/**
 * SeedJoy - BLE Configuration Service
 * 
 * Custom GATT service for device configuration over BLE
 */

#ifndef BLE_CONFIG_H
#define BLE_CONFIG_H

#include "config.h"
#include "storage.h"
#include "axes.h"
#include "buttons.h"
#include <bluefruit.h>

// Custom service UUID (e95d0001-251d-470a-a062-fa1922dfa9a8)
#define BLE_CONFIG_SERVICE_UUID "e95d0001-251d-470a-a062-fa1922dfa9a8"

// Characteristic UUIDs
#define BLE_CONFIG_READ_UUID    "e95d0002-251d-470a-a062-fa1922dfa9a8"  // Read full config
#define BLE_CONFIG_WRITE_UUID   "e95d0003-251d-470a-a062-fa1922dfa9a8"  // Write full config
#define BLE_STATUS_UUID         "e95d0004-251d-470a-a062-fa1922dfa9a8"  // Device status
#define BLE_AXES_MONITOR_UUID   "e95d0005-251d-470a-a062-fa1922dfa9a8"  // Real-time axis values
#define BLE_BUTTONS_MONITOR_UUID "e95d0007-251d-470a-a062-fa1922dfa9a8"  // Real-time button states
#define BLE_CALIBRATE_UUID      "e95d0006-251d-470a-a062-fa1922dfa9a8"  // Calibration commands

// Maximum config size (JSON serialized)
#define MAX_CONFIG_SIZE 2048

// Calibration command types
enum CalibrationCommand {
  CAL_MIN = 0,
  CAL_CENTER = 1,
  CAL_MAX = 2,
  CAL_SAVE = 3,
  CAL_RESET = 4
};

class BLEConfigService {
public:
  BLEConfigService();
  
  // Initialize service (call after BLE is initialized)
  bool begin(DeviceConfig* config, StorageManager* storage, AxesProcessor* axes);
  
  // Update axis monitoring (call in main loop)
  void updateAxisMonitoring();
  
  // Set axis processor for real-time monitoring
  void setAxesProcessor(AxesProcessor* axes);
  // Set buttons processor for real-time button monitoring
  void setButtonsProcessor(ButtonsProcessor* buttons);
  
  // Get service instance
  BLEService& getService();
  
private:
  // GATT Service
  BLEService service_;
  
  // Characteristics
  BLECharacteristic configReadChar_;
  BLECharacteristic configWriteChar_;
  BLECharacteristic statusChar_;
  BLECharacteristic axesMonitorChar_;
  BLECharacteristic buttonsMonitorChar_;
  BLECharacteristic calibrateChar_;
  
  // Pointers to device objects
  DeviceConfig* config_;
  StorageManager* storage_;
  AxesProcessor* axes_;
  ButtonsProcessor* buttons_;
  
  // Data buffers
  uint8_t configBuffer_[MAX_CONFIG_SIZE];
  uint16_t configBufferSize_;
  
  // Axis monitoring data
  struct __attribute__((packed)) AxisMonitorData {
    int16_t processed[MAX_AXES];  // Processed axis values
    uint16_t raw[MAX_AXES];       // Raw ADC values
    uint32_t timestamp;           // Milliseconds since boot
  };
  AxisMonitorData axisData_;

  // Button monitoring data (16-button bitmask + timestamp)
  struct __attribute__((packed)) ButtonsMonitorData {
    uint16_t bitmask;    // Button bitmask (bit 0..15)
    uint32_t timestamp;  // Milliseconds since boot
  };
  ButtonsMonitorData buttonsData_;
  
  // Callbacks
  static void configWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
  static void calibrateCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
  
  // Helper functions
  bool serializeConfig();
  bool deserializeConfig(const uint8_t* data, uint16_t len);
  void sendStatus(const char* message, bool success);
  
  // Static instance for callbacks
  static BLEConfigService* instance_;
};

#endif // BLE_CONFIG_H
