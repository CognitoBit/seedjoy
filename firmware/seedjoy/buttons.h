/**
 * SeedJoy - Button Processing
 * 
 * Handles button debouncing, logical mapping, and state management
 * Supports both direct GPIO buttons and shift register (74HC165) buttons
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include "config.h"
#include "shift_registers.h"

class ButtonsProcessor {
public:
  ButtonsProcessor();
  
  // Initialize with configuration
  void begin(const DeviceConfig* config);
  
  // Update configuration
  void setConfig(const DeviceConfig* config);
  
  // Read and debounce all buttons (call in main loop)
  void update();
  
  // Get button state by logical number (0-143: 16 GPIO + 128 shift register)
  bool isPressed(uint8_t logicalNumber);
  
  // Get all button states as bitmask (bit 0 = button 0, etc.)
  uint16_t getButtonBitmask();
  
  // Get extended button states (for buttons > 16, returns array of bitmasks)
  void getExtendedButtonStates(uint8_t* bitmasks, uint8_t maxBytes);
  
  // Get total button count (GPIO + shift register)
  uint8_t getTotalButtonCount();
  
  // Get physical button state (before mapping, for calibration)
  bool getPhysicalState(uint8_t buttonIndex);
  
private:
  const DeviceConfig* config_;
  
  // Shift register reader
  ShiftRegisterReader shiftRegisters_;
  
  // Logical button states (after mapping) - expanded for shift registers
  bool logicalStates_[MAX_TOTAL_BUTTONS];
  
  // Physical button states (current reading) - GPIO only
  bool physicalStates_[MAX_BUTTONS];
  
  // Debounce tracking - GPIO
  uint32_t lastChangeTime_[MAX_BUTTONS];
  bool debouncedStates_[MAX_BUTTONS];

  // Debounce tracking - shift register buttons (mechanical switches, not
  // hardware debounced; without this, contact chatter double-fires at report rate)
  uint32_t srLastChangeTime_[MAX_SHIFT_REGISTER_BUTTONS];
  bool srDebouncedStates_[MAX_SHIFT_REGISTER_BUTTONS];

  // Total button count (GPIO + shift register)
  uint8_t totalButtonCount_;

  // Logical offset for SR buttons:
  // 0 when no GPIO buttons are enabled (SR-only mode)
  // MAX_BUTTONS when GPIO buttons are also active
  uint8_t srOffset_;
  
  // Debounce time in milliseconds
  static const uint32_t DEBOUNCE_MS = 5;
  
  // Read physical pin state
  bool readPhysicalPin(uint8_t pin, bool inverted);
};

#endif // BUTTONS_H
