/**
 * SeedJoy - Button Processing Implementation
 */

#include "buttons.h"
#include <Arduino.h>

ButtonsProcessor::ButtonsProcessor() : config_(nullptr), totalButtonCount_(MAX_BUTTONS), srOffset_(0) {
  for (int i = 0; i < MAX_TOTAL_BUTTONS; i++) {
    logicalStates_[i] = false;
  }
  for (int i = 0; i < MAX_BUTTONS; i++) {
    physicalStates_[i] = false;
    debouncedStates_[i] = false;
    lastChangeTime_[i] = 0;
  }
}

void ButtonsProcessor::begin(const DeviceConfig* config) {
  config_ = config;
  
  // Configure button pins as inputs with pull-ups
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (config_->buttons[i].enabled && config_->buttons[i].pin != 0xFF) {
      pinMode(config_->buttons[i].pin, INPUT_PULLUP);
    }
  }
  
  // Initialize shift registers if enabled
  shiftRegisters_.begin(&config_->shiftRegisters);

  // Determine SR offset: 0 if no GPIO buttons are enabled (SR-only mode),
  // MAX_BUTTONS if any GPIO buttons are active (mixed mode)
  srOffset_ = 0;
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (config_->buttons[i].enabled) {
      srOffset_ = MAX_BUTTONS;
      break;
    }
  }

  // Calculate total button count
  if (config_->shiftRegisters.enabled) {
    totalButtonCount_ = srOffset_ + shiftRegisters_.getButtonCount();
    if (totalButtonCount_ > MAX_TOTAL_BUTTONS) totalButtonCount_ = MAX_TOTAL_BUTTONS;
  } else {
    totalButtonCount_ = MAX_BUTTONS;
  }
  
  Serial.print("Total buttons: ");
  Serial.print(totalButtonCount_);
  Serial.print(" (GPIO: ");
  Serial.print(MAX_BUTTONS);
  Serial.print(", Shift Reg: ");
  Serial.print(totalButtonCount_ - MAX_BUTTONS);
  Serial.println(")");
}

void ButtonsProcessor::setConfig(const DeviceConfig* config) {
  config_ = config;
  // Propagate to the embedded SR reader so numChips / pins stay in sync
  shiftRegisters_.setConfig(&config_->shiftRegisters);
}

void ButtonsProcessor::update() {
  unsigned long now = millis();
  
  // Clear all logical states
  for (int i = 0; i < MAX_TOTAL_BUTTONS; i++) {
    logicalStates_[i] = false;
  }
  
  // Process GPIO buttons
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (!config_->buttons[i].enabled) {
      physicalStates_[i] = false;
      debouncedStates_[i] = false;
      continue;
    }
    
    // Read current physical state
    bool currentState = readPhysicalPin(
      config_->buttons[i].pin,
      config_->buttons[i].inverted
    );
    
    physicalStates_[i] = currentState;
    
    // Debouncing logic
    if (currentState != debouncedStates_[i]) {
      // State has changed - check if enough time has passed
      if (now - lastChangeTime_[i] >= DEBOUNCE_MS) {
        // Debounce time passed, accept the new state
        debouncedStates_[i] = currentState;
        lastChangeTime_[i] = now;
      }
    } else {
      // State is stable, update last change time
      lastChangeTime_[i] = now;
    }
    
    // Map physical button to logical button
    uint8_t logicalNum = config_->buttons[i].logicalNumber;
    if (logicalNum < MAX_TOTAL_BUTTONS) {
      logicalStates_[logicalNum] = debouncedStates_[i];
    }
  }
  
  // Update shift register buttons
  if (config_->shiftRegisters.enabled) {
    shiftRegisters_.update();
    
    // Map shift register buttons to logical buttons
    // srOffset_ = 0 in SR-only mode; MAX_BUTTONS when GPIO buttons are also active
    uint8_t srButtonCount = shiftRegisters_.getButtonCount();
    for (uint8_t i = 0; i < srButtonCount; i++) {
      uint8_t logicalNum = srOffset_ + i;
      if (logicalNum < MAX_TOTAL_BUTTONS) {
        logicalStates_[logicalNum] = shiftRegisters_.getButtonState(i);
      }
    }
  }
}

bool ButtonsProcessor::isPressed(uint8_t logicalNum) {
  if (logicalNum >= MAX_TOTAL_BUTTONS) return false;
  return logicalStates_[logicalNum];
}

uint16_t ButtonsProcessor::getButtonBitmask() {
  uint16_t bitmask = 0;
  
  // Only return first 16 buttons for compatibility
  for (int i = 0; i < 16 && i < MAX_TOTAL_BUTTONS; i++) {
    if (logicalStates_[i]) {
      bitmask |= (1 << i);
    }
  }
  
  return bitmask;
}

void ButtonsProcessor::getExtendedButtonStates(uint8_t* bitmasks, uint8_t maxBytes) {
  uint8_t numBytes = (totalButtonCount_ + 7) / 8;  // Round up to nearest byte
  if (numBytes > maxBytes) numBytes = maxBytes;
  
  // Clear output
  memset(bitmasks, 0, maxBytes);
  
  // Pack button states into byte array
  for (uint8_t i = 0; i < totalButtonCount_; i++) {
    uint8_t byteIndex = i / 8;
    uint8_t bitIndex = i % 8;
    
    if (byteIndex < numBytes && logicalStates_[i]) {
      bitmasks[byteIndex] |= (1 << bitIndex);
    }
  }
}

uint8_t ButtonsProcessor::getTotalButtonCount() {
  return totalButtonCount_;
}

bool ButtonsProcessor::getPhysicalState(uint8_t buttonIndex) {
  if (buttonIndex >= MAX_BUTTONS) return false;
  return physicalStates_[buttonIndex];
}

bool ButtonsProcessor::readPhysicalPin(uint8_t pin, bool inverted) {
  if (pin == 0xFF) return false;
  
  // Read pin (INPUT_PULLUP: LOW = pressed by default)
  bool state = digitalRead(pin) == LOW;
  
  // Apply inversion if configured
  if (inverted) {
    state = !state;
  }
  
  return state;
}
