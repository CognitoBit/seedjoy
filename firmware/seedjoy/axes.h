/**
 * SeedJoy - Analog Axes Processing
 * 
 * Handles reading, calibration, filtering, and curve application
 * for analog axes (potentiometers, joysticks, etc.)
 */

#ifndef AXES_H
#define AXES_H

#include "config.h"

class AxesProcessor {
public:
  AxesProcessor();
  
  // Initialize with configuration
  void begin(const DeviceConfig* config);
  
  // Update configuration (e.g., after loading from Flash)
  void setConfig(const DeviceConfig* config);
  
  // Read and process all axes (call in main loop)
  void update();
  
  // Get processed axis value (-32767 to +32767)
  int16_t getAxisValue(uint8_t axisIndex);
  
  // Get raw ADC reading (for calibration)
  uint16_t getRawValue(uint8_t axisIndex);
  
  // Auto-calibrate axis (call min/center/max during calibration wizard)
  void calibrateMin(uint8_t axisIndex);
  void calibrateCenter(uint8_t axisIndex);
  void calibrateMax(uint8_t axisIndex);
  
private:
  const DeviceConfig* config_;
  
  // Processed values
  int16_t processedValues_[MAX_AXES];
  
  // Raw ADC values (12-bit: 0-4095)
  uint16_t rawValues_[MAX_AXES];
  
  // Smoothing buffers
  uint16_t smoothBuffer_[MAX_AXES][8];
  uint8_t smoothIndex_[MAX_AXES];
  
  // ADC resolution
  static const uint16_t ADC_MAX = 4095;
  static const int16_t OUTPUT_MIN = -32767;
  static const int16_t OUTPUT_MAX = 32767;
  
  // Internal processing functions
  uint16_t readRawADC(uint8_t pin);
  uint16_t applySmoothing(uint8_t axisIndex, uint16_t rawValue);
  float normalizeRaw(uint16_t raw, const AxisConfig& axisCfg);
  float applyDeadzone(float normalized, uint8_t deadzonePercent);
  float applyCurve(float input, const AxisConfig& axisCfg);
  int16_t scaleToOutput(float normalized, bool inverted);
};

#endif // AXES_H
