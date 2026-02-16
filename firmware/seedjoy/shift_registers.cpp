/**
 * SeedJoy - Shift Register Reader Implementation
 */

#include "shift_registers.h"
#include <Arduino.h>

ShiftRegisterReader::ShiftRegisterReader() : config_(nullptr), numButtons_(0) {
  memset(buttonData_, 0, sizeof(buttonData_));
}

void ShiftRegisterReader::begin(const ShiftRegisterConfig* config) {
  config_ = config;
  
  if (!config_ || !config_->enabled) {
    numButtons_ = 0;
    return;
  }
  
  // Calculate number of buttons
  numButtons_ = config_->numChips * 8;
  if (numButtons_ > MAX_SHIFT_REGISTER_BUTTONS) {
    numButtons_ = MAX_SHIFT_REGISTER_BUTTONS;
  }
  
  // Configure pins
  if (config_->dataPin != 0xFF) {
    pinMode(config_->dataPin, INPUT);
  }
  if (config_->clockPin != 0xFF) {
    pinMode(config_->clockPin, OUTPUT);
    digitalWrite(config_->clockPin, LOW);
  }
  if (config_->loadPin != 0xFF) {
    pinMode(config_->loadPin, OUTPUT);
    digitalWrite(config_->loadPin, HIGH);
  }
  
  Serial.print("Shift registers initialized: ");
  Serial.print(config_->numChips);
  Serial.print(" chips, ");
  Serial.print(numButtons_);
  Serial.println(" buttons");
}

void ShiftRegisterReader::setConfig(const ShiftRegisterConfig* config) {
  config_ = config;
  if (config_ && config_->enabled) {
    numButtons_ = config_->numChips * 8;
    if (numButtons_ > MAX_SHIFT_REGISTER_BUTTONS) {
      numButtons_ = MAX_SHIFT_REGISTER_BUTTONS;
    }
  } else {
    numButtons_ = 0;
  }
}

void ShiftRegisterReader::update() {
  if (!config_ || !config_->enabled || numButtons_ == 0) {
    return;
  }
  
  readShiftRegisters();
}

bool ShiftRegisterReader::getButtonState(uint8_t index) {
  if (index >= numButtons_) return false;
  
  uint8_t byteIndex = index / 8;
  uint8_t bitIndex = index % 8;
  
  bool state = (buttonData_[byteIndex] >> bitIndex) & 0x01;
  
  // Apply inversion if configured
  if (config_->inverted) {
    state = !state;
  }
  
  return state;
}

void ShiftRegisterReader::getButtonStates(bool* states, uint8_t maxStates) {
  uint8_t count = (maxStates < numButtons_) ? maxStates : numButtons_;
  
  for (uint8_t i = 0; i < count; i++) {
    states[i] = getButtonState(i);
  }
}

uint8_t ShiftRegisterReader::getButtonCount() {
  return numButtons_;
}

void ShiftRegisterReader::readShiftRegisters() {
  if (config_->dataPin == 0xFF || 
      config_->clockPin == 0xFF || 
      config_->loadPin == 0xFF) {
    return;
  }
  
  // Pulse load pin LOW to latch current button states
  digitalWrite(config_->loadPin, LOW);
  delayMicroseconds(5);  // tSU (setup time): min 20ns for HC165
  digitalWrite(config_->loadPin, HIGH);
  delayMicroseconds(5);  // tH (hold time): min 5ns for HC165
  
  // Read data from shift registers
  // Data is shifted MSB first, starting from the last chip in the chain
  for (int chip = config_->numChips - 1; chip >= 0; chip--) {
    uint8_t byteData = 0;
    
    // Read 8 bits (one byte per chip)
    for (int bit = 0; bit < 8; bit++) {
      // Read current bit
      uint8_t bitValue = digitalRead(config_->dataPin);
      
      // Shift bit into byte (MSB first)
      byteData |= (bitValue << (7 - bit));
      
      // Pulse clock HIGH to shift next bit
      digitalWrite(config_->clockPin, HIGH);
      delayMicroseconds(1);  // tCLK (clock pulse width): min 25ns for HC165
      digitalWrite(config_->clockPin, LOW);
      delayMicroseconds(1);
    }
    
    // Store byte for this chip
    buttonData_[chip] = byteData;
  }
}
