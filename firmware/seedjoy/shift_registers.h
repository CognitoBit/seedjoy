/**
 * SeedJoy - Shift Register Reader (74HC165 / CD4021)
 * 
 * Reads button states from daisy-chained shift registers
 * Compatible with 74HC165, 74HCT165, CD4021B
 */

#ifndef SHIFT_REGISTERS_H
#define SHIFT_REGISTERS_H

#include "config.h"

class ShiftRegisterReader {
public:
  ShiftRegisterReader();
  
  // Initialize shift registers
  void begin(const ShiftRegisterConfig* config);
  
  // Update configuration
  void setConfig(const ShiftRegisterConfig* config);
  
  // Read all shift register buttons
  void update();
  
  // Get button state by index (0 = first button in chain)
  bool getButtonState(uint8_t index);
  
  // Get all button states as bitmask array
  void getButtonStates(bool* states, uint8_t maxStates);
  
  // Get number of available buttons
  uint8_t getButtonCount();
  
private:
  const ShiftRegisterConfig* config_;
  
  // Button states (up to 128 buttons = 16 bytes)
  uint8_t buttonData_[MAX_SHIFT_REGISTERS];
  uint8_t numButtons_;
  
  // Read shift register chain
  void readShiftRegisters();
};

#endif // SHIFT_REGISTERS_H
