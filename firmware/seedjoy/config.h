/**
 * SeedJoy - Configuration Data Structures
 * 
 * Defines the device configuration structure that is stored in Flash
 * and synchronized with the web configurator.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "ffb_config.h"   // ENABLE_FFB gate (Arduino-free)

// Firmware version
#define FIRMWARE_VERSION_MAJOR 0
#define FIRMWARE_VERSION_MINOR 1
#define FIRMWARE_VERSION_PATCH 0

// Hardware constraints
#define MAX_AXES 4
#define MAX_BUTTONS 16
#define MAX_SHIFT_REGISTER_BUTTONS 128  // Up to 16 shift registers * 8 bits
#define MAX_TOTAL_BUTTONS (MAX_BUTTONS + MAX_SHIFT_REGISTER_BUTTONS)
#define MAX_SHIFT_REGISTERS 16
#define MAX_CURVE_POINTS 5

// Pin definitions for XIAO nRF52840
// Reference: https://wiki.seeedstudio.com/XIAO_BLE/

// Mode select button (moved from D0 to avoid conflict with A0)
#define MODE_SELECT_PIN D9  // P1.14 (MISO) - Hold LOW for BLE, HIGH for USB on boot
#define STATUS_LED_PIN LED_RED    // P0.26 (red LED)
#define CONNECTION_LED_PIN LED_BLUE  // P0.06 (blue LED)
// The XIAO nRF52840's RGB LED is common-anode: LOW = lit. (The Seeed variant's
// LED_STATE_ON is 1, which is wrong for this board, so don't rely on it.)
#define LED_ON  LOW
#define LED_OFF HIGH

// Analog axes (4 axes on dedicated ADC pins)
#define AXIS_0_PIN A0  // D0 = P0.02 (ADC1)
#define AXIS_1_PIN A1  // D1 = P0.03 (ADC2)
#define AXIS_2_PIN A2  // D2 = P0.28 (ADC5)
#define AXIS_3_PIN A3  // D3 = P0.29 (ADC6)

// Default shift register pins (74HC165 parallel-in, serial-out)
#define SR_DATA_PIN  D6   // P1.11 (TX) - Serial data out (Q7)
#define SR_CLOCK_PIN D7   // P1.12 (RX) - Shift clock
#define SR_LOAD_PIN  D8   // P1.13 (SCK) - Parallel load (active LOW)

// Legacy direct button pins (optional - primarily using shift registers)
// Only 3 pins allocated, rest disabled (0xFF)
const uint8_t DEFAULT_BUTTON_PINS[MAX_BUTTONS] = {
  D4,   // P0.04 (SDA/A4)
  D5,   // P0.05 (SCL/A5)
  D10,  // P1.15 (MOSI)
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Unused
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF
};

// Operation modes
enum OperationMode {
  MODE_USB = 0,
  MODE_BLE = 1,
  MODE_AUTO = 2  // Future: auto-switch based on USB connection
};

// Axis curve types
enum AxisCurve {
  CURVE_LINEAR = 0,
  CURVE_EXPO = 1,
  CURVE_CUSTOM = 2
};

// Axis configuration
struct AxisConfig {
  uint8_t pin;              // ADC pin number
  bool enabled;             // Is this axis enabled?
  bool inverted;            // Invert axis direction
  uint16_t min;             // Calibrated minimum (0-4095)
  uint16_t center;          // Calibrated center (0-4095)
  uint16_t max;             // Calibrated maximum (0-4095)
  uint8_t deadzone;         // Deadzone percentage (0-50)
  AxisCurve curveType;      // Curve type
  float expoFactor;         // Expo curve factor (0.0-1.0)
  float customCurve[MAX_CURVE_POINTS]; // Custom curve points (0.0-1.0)
  uint8_t smoothing;        // Smoothing samples (0=off, 2,4,8)
  
  // Defaults
  AxisConfig() : 
    pin(0xFF), enabled(false), inverted(false),
    min(0), center(2048), max(4095),
    deadzone(5), curveType(CURVE_LINEAR), 
    expoFactor(0.0f), smoothing(0) {
      for (int i = 0; i < MAX_CURVE_POINTS; i++) {
        customCurve[i] = i / (float)(MAX_CURVE_POINTS - 1);
      }
    }
};

// Button configuration
struct ButtonConfig {
  uint8_t pin;              // GPIO pin number
  bool enabled;             // Is this button enabled?
  uint8_t logicalNumber;    // Logical button number (0-15)
  bool inverted;            // Invert logic (normally HIGH = pressed)
  
  // Defaults
  ButtonConfig() : 
    pin(0xFF), enabled(false), 
    logicalNumber(0), inverted(false) {}
};

// Shift Register configuration (74HC165 / CD4021)
struct ShiftRegisterConfig {
  bool enabled;             // Shift registers enabled
  uint8_t numChips;         // Number of chained shift registers (1-16)
  uint8_t dataPin;          // Data pin (SER_OUT / Q7)
  uint8_t clockPin;         // Clock pin (CLK)
  uint8_t loadPin;          // Latch/Load pin (SH/LD)
  bool inverted;            // true = input LOW means pressed (pull-up + switch to GND)
  
  // Defaults (using predefined SR_ pins)
  // SR-only mode: 7 chips = 56 buttons (covers up to ~50 physical buttons)
  ShiftRegisterConfig() :
    enabled(true), numChips(7),
    dataPin(D6), clockPin(D7), loadPin(D8),
    // Standard wiring is a 10k pull-up per input with the switch to GND, so a
    // pressed button reads LOW -> invert so pressed = 1 in the HID report.
    inverted(true) {}
};

// Device configuration (stored in Flash)
struct DeviceConfig {
  // Magic number for validation
  uint32_t magic;           // 0x5EED70B7 ("SEEDJOB" in hex-ish)
  
  // Version
  uint8_t configVersion;    // Config structure version (1)
  uint8_t firmwareMajor;
  uint8_t firmwareMinor;
  uint8_t firmwarePatch;
  
  // Device settings
  char deviceName[32];      // BLE device name
  OperationMode mode;       // USB, BLE, or AUTO
  
  // Shift register configuration
  ShiftRegisterConfig shiftRegisters;
  
  // USB HID settings
  uint16_t usbVID;          // Vendor ID (0x239A = Adafruit)
  uint16_t usbPID;          // Product ID
  uint8_t usbPollRate;      // Poll rate in ms (1 = 1000Hz)
  
  // BLE settings
  uint16_t bleConnInterval; // Connection interval in 1.25ms units (min 6 = 7.5ms per BLE spec)
  int8_t bleTxPower;        // TX power in dBm (-40 to +4)
  
  // Axis configuration
  AxisConfig axes[MAX_AXES];
  
  // Button configuration
  ButtonConfig buttons[MAX_BUTTONS];
  
  // Power management
  bool autoSleep;           // Auto-sleep in BLE mode
  uint16_t sleepTimeout;    // Sleep timeout in seconds
  
  // CRC for validation
  uint32_t crc32;
  
  // Constructor with defaults
  DeviceConfig() {
    magic = 0x5EED70B7;
    configVersion = 1;
    firmwareMajor = FIRMWARE_VERSION_MAJOR;
    firmwareMinor = FIRMWARE_VERSION_MINOR;
    firmwarePatch = FIRMWARE_VERSION_PATCH;
    
    strcpy(deviceName, "SeedJoy");
    mode = MODE_BLE;
    
    usbVID = 0x239A;  // Adafruit VID
    usbPID = 0x80F4;  // Generic HID
    usbPollRate = 1;
    
    bleConnInterval = 6;  // 6 * 1.25ms = 7.5ms (BLE minimum; lowest legal latency)
    bleTxPower = 0;       // 0 dBm
    
    // Initialize default axes (DISABLED by default to prevent floating pin noise)
    // SAFETY: User must explicitly enable axes in configurator after connecting hardware
    // Floating pins read electrical noise and cause random HID input!
    for (int i = 0; i < MAX_AXES; i++) {
      axes[i].enabled = false;  // SAFETY: disabled until user enables in configurator
      if (i == 0) axes[i].pin = AXIS_0_PIN;
      else if (i == 1) axes[i].pin = AXIS_1_PIN;
      else if (i == 2) axes[i].pin = AXIS_2_PIN;
      else if (i == 3) axes[i].pin = AXIS_3_PIN;
    }
    
    // Initialize default buttons (DISABLED by default to prevent floating pin noise)
    // SAFETY: User must explicitly enable buttons in configurator after connecting hardware
    for (int i = 0; i < MAX_BUTTONS; i++) {
      buttons[i].enabled = false;  // SAFETY: disabled until user enables in configurator
      buttons[i].pin = DEFAULT_BUTTON_PINS[i];
      buttons[i].logicalNumber = i;
    }
    
    autoSleep = false;
    sleepTimeout = 600;  // 10 minutes
    
    crc32 = 0;
  }
};

// Calculate CRC32 for config validation
uint32_t calculateCRC32(const DeviceConfig* config);

// Validate config structure
bool validateConfig(const DeviceConfig* config);

#endif // CONFIG_H
