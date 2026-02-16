/**
 * SeedJoy - Analog Axes Processing Implementation
 */

#include "axes.h"
#include <Arduino.h>

AxesProcessor::AxesProcessor() : config_(nullptr) {
  for (int i = 0; i < MAX_AXES; i++) {
    processedValues_[i] = 0;
    rawValues_[i] = 0;
    smoothIndex_[i] = 0;
    for (int j = 0; j < 8; j++) {
      smoothBuffer_[i][j] = 0;
    }
  }
}

void AxesProcessor::begin(const DeviceConfig* config) {
  config_ = config;
  
  // Configure ADC resolution (12-bit for nRF52)
  analogReadResolution(12);
  
  // Initialize axis pins
  for (int i = 0; i < MAX_AXES; i++) {
    if (config_->axes[i].enabled && config_->axes[i].pin != 0xFF) {
      pinMode(config_->axes[i].pin, INPUT);
    }
  }
}

void AxesProcessor::setConfig(const DeviceConfig* config) {
  config_ = config;
}

void AxesProcessor::update() {
  if (!config_) return;
  
  for (int i = 0; i < MAX_AXES; i++) {
    if (!config_->axes[i].enabled) {
      processedValues_[i] = 0;
      continue;
    }
    
    // 1. Read raw ADC value
    uint16_t raw = readRawADC(config_->axes[i].pin);
    rawValues_[i] = raw;
    
    // 2. Apply smoothing
    if (config_->axes[i].smoothing > 0) {
      raw = applySmoothing(i, raw);
    }
    
    // 3. Normalize using calibration (0.0 to 1.0, centered at 0.5)
    float normalized = normalizeRaw(raw, config_->axes[i]);
    
    // 4. Apply deadzone
    if (config_->axes[i].deadzone > 0) {
      normalized = applyDeadzone(normalized, config_->axes[i].deadzone);
    }
    
    // 5. Apply curve
    normalized = applyCurve(normalized, config_->axes[i]);
    
    // 6. Scale to output range
    processedValues_[i] = scaleToOutput(normalized, config_->axes[i].inverted);
  }
}

int16_t AxesProcessor::getAxisValue(uint8_t axisIndex) {
  if (axisIndex >= MAX_AXES) return 0;
  return processedValues_[axisIndex];
}

uint16_t AxesProcessor::getRawValue(uint8_t axisIndex) {
  if (axisIndex >= MAX_AXES) return 0;
  return rawValues_[axisIndex];
}

void AxesProcessor::calibrateMin(uint8_t axisIndex) {
  if (axisIndex >= MAX_AXES || !config_) return;
  // This would update the config - in practice, call from main sketch
  // config_->axes[axisIndex].min = rawValues_[axisIndex];
}

void AxesProcessor::calibrateCenter(uint8_t axisIndex) {
  if (axisIndex >= MAX_AXES || !config_) return;
  // config_->axes[axisIndex].center = rawValues_[axisIndex];
}

void AxesProcessor::calibrateMax(uint8_t axisIndex) {
  if (axisIndex >= MAX_AXES || !config_) return;
  // config_->axes[axisIndex].max = rawValues_[axisIndex];
}

uint16_t AxesProcessor::readRawADC(uint8_t pin) {
  if (pin == 0xFF) return 0;
  return analogRead(pin);
}

uint16_t AxesProcessor::applySmoothing(uint8_t axisIndex, uint16_t rawValue) {
  uint8_t samples = config_->axes[axisIndex].smoothing;
  if (samples == 0 || samples > 8) return rawValue;
  
  // Add to circular buffer
  smoothBuffer_[axisIndex][smoothIndex_[axisIndex]] = rawValue;
  smoothIndex_[axisIndex] = (smoothIndex_[axisIndex] + 1) % samples;
  
  // Calculate average
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += smoothBuffer_[axisIndex][i];
  }
  
  return sum / samples;
}

float AxesProcessor::normalizeRaw(uint16_t raw, const AxisConfig& axisCfg) {
  // Normalize to 0.0-1.0 range using calibration
  float normalized;
  
  if (raw <= axisCfg.center) {
    // Lower half (min to center) -> 0.0 to 0.5
    if (axisCfg.center == axisCfg.min) {
      normalized = 0.5f;
    } else {
      normalized = 0.5f * (float)(raw - axisCfg.min) / (float)(axisCfg.center - axisCfg.min);
    }
  } else {
    // Upper half (center to max) -> 0.5 to 1.0
    if (axisCfg.max == axisCfg.center) {
      normalized = 0.5f;
    } else {
      normalized = 0.5f + 0.5f * (float)(raw - axisCfg.center) / (float)(axisCfg.max - axisCfg.center);
    }
  }
  
  // Clamp to 0.0-1.0
  if (normalized < 0.0f) normalized = 0.0f;
  if (normalized > 1.0f) normalized = 1.0f;
  
  return normalized;
}

float AxesProcessor::applyDeadzone(float normalized, uint8_t deadzonePercent) {
  // Center deadzone around 0.5 (center position)
  float deadzone = deadzonePercent / 100.0f / 2.0f;  // Half on each side
  float center = 0.5f;
  
  float distance = normalized - center;
  
  if (fabs(distance) < deadzone) {
    return center;  // In deadzone, return center
  }
  
  // Scale outside deadzone to full range
  if (distance > 0) {
    // Upper half
    return center + (distance - deadzone) * (0.5f / (0.5f - deadzone));
  } else {
    // Lower half
    return center + (distance + deadzone) * (0.5f / (0.5f - deadzone));
  }
}

float AxesProcessor::applyCurve(float input, const AxisConfig& axisCfg) {
  switch (axisCfg.curveType) {
    case CURVE_LINEAR:
      return input;
      
    case CURVE_EXPO:
      {
        // Exponential curve: output = input^(1+expo)
        // expo: 0.0 = linear, 1.0 = quadratic
        float centered = (input - 0.5f) * 2.0f;  // -1.0 to 1.0
        float sign = (centered >= 0) ? 1.0f : -1.0f;
        float expo = axisCfg.expoFactor;
        float curved = sign * pow(fabs(centered), 1.0f + expo);
        return (curved + 1.0f) / 2.0f;  // Back to 0.0-1.0
      }
      
    case CURVE_CUSTOM:
      {
        // Linear interpolation between custom curve points
        float scaledInput = input * (MAX_CURVE_POINTS - 1);
        int idx = (int)scaledInput;
        if (idx >= MAX_CURVE_POINTS - 1) return axisCfg.customCurve[MAX_CURVE_POINTS - 1];
        
        float fraction = scaledInput - idx;
        float v1 = axisCfg.customCurve[idx];
        float v2 = axisCfg.customCurve[idx + 1];
        
        return v1 + (v2 - v1) * fraction;
      }
      
    default:
      return input;
  }
}

int16_t AxesProcessor::scaleToOutput(float normalized, bool inverted) {
  // Convert 0.0-1.0 to OUTPUT_MIN to OUTPUT_MAX
  int16_t output = (int16_t)(normalized * (OUTPUT_MAX - OUTPUT_MIN) + OUTPUT_MIN);
  
  if (inverted) {
    output = -output;
  }
  
  return output;
}
