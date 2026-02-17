/**
 * SeedJoy - Main Firmware
 * 
 * Dual-mode (USB/BLE) game controller for Seeed Studio XIAO nRF52840
 * 
 * Features:
 * - 4 analog axes with calibration and curves
 * - 16 digital buttons with debouncing
 * - Manual mode selection (USB or BLE)
 * - Web-based configuration via BLE
 * - Persistent configuration in Flash
 * 
 * Hardware:
 * - Seeed Studio XIAO nRF52840
 * - 4x potentiometers on A0-A3
 * - 16x buttons on digital pins
 * - Mode select button on D0
 */

// Development flags
#define DEV_FORCE_BLE_MODE 1  // Set to 1 to force BLE mode, 0 for normal operation

#include "config.h"
#include "storage.h"
#include "axes.h"
#include "buttons.h"
#include "usb_hid.h"
#include "ble_hid.h"
#include "ble_config.h"

// Global objects
DeviceConfig deviceConfig;
StorageManager storage;
AxesProcessor axes;
ButtonsProcessor buttons;
USBHIDController usbHID;
BLEHIDController bleHID;
BLEConfigService bleConfig;

// Current operation mode
OperationMode currentMode = MODE_USB;

// Configuration mode for BLE (prevents HID reports during setup)
bool configurationMode = false;
uint32_t bleConnectionTime = 0;
const uint32_t CONFIG_MODE_TIMEOUT = 60000; // 60 seconds after connection (was 10s - extended for safety)
bool wasConnected = false;

// Battery monitoring
#define VBAT_PIN PIN_VBAT
#define VBAT_MV_PER_LSB (0.73242188F)  // 3.0V ADC range / 4096
#define VBAT_DIVIDER (0.5F)            // Voltage divider
#define VBAT_DIVIDER_COMP (2.0F)       // Compensation factor

// Timing
uint32_t lastReportTime = 0;
uint32_t lastBatteryCheck = 0;
const uint32_t BATTERY_CHECK_INTERVAL = 60000; // Check every 60 seconds

// Function prototypes
void selectOperationMode();
void initializeHardware();
void updateInputs();
void sendHIDReports();
void updateBatteryLevel();
float readVBAT();
void blinkStatus(int times);

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  
  // Wait a bit for serial to initialize (optional)
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("SeedJoy - BLE/USB Game Controller");
  Serial.print("Firmware v");
  Serial.print(FIRMWARE_VERSION_MAJOR);
  Serial.print(".");
  Serial.print(FIRMWARE_VERSION_MINOR);
  Serial.print(".");
  Serial.println(FIRMWARE_VERSION_PATCH);
  Serial.println("=================================");
  
  // Initialize LEDs
  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(CONNECTION_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);  // Turn on status LED during init
  digitalWrite(CONNECTION_LED_PIN, LOW);
  
  // Initialize storage
  Serial.println("Initializing storage...");
  storage.begin();
  
  // Load configuration
  Serial.println("Loading configuration...");
  if (!storage.loadConfig(&deviceConfig)) {
    Serial.println("Using default configuration");
    // Save default config
    storage.saveConfig(&deviceConfig);
  }
  
  Serial.print("Device name: ");
  Serial.println(deviceConfig.deviceName);
  Serial.print("Config mode: ");
  Serial.println(deviceConfig.mode == MODE_USB ? "USB" : 
                 deviceConfig.mode == MODE_BLE ? "BLE" : "AUTO");
  
  // Select operation mode (read mode select button)
  selectOperationMode();
  
  // Initialize hardware components
  initializeHardware();
  
  // Initialize appropriate HID interface
  if (currentMode == MODE_USB) {
    Serial.println("Starting USB HID...");
    if (usbHID.begin(&deviceConfig)) {
      Serial.println("USB HID initialized successfully");
      digitalWrite(STATUS_LED_PIN, LOW);   // Turn off status LED
      digitalWrite(CONNECTION_LED_PIN, HIGH); // Turn on connection LED
    } else {
      Serial.println("USB HID initialization failed!");
      blinkStatus(5); // Blink 5 times to indicate error
    }
  } else if (currentMode == MODE_BLE) {
    Serial.println("Starting BLE HID...");
    if (bleHID.begin(&deviceConfig)) {
      Serial.println("BLE HID initialized successfully");
      
      // Initialize BLE config service BEFORE starting advertising
      Serial.println("Starting BLE Config Service...");
      if (bleConfig.begin(&deviceConfig, &storage, &axes)) {
        Serial.println("BLE Config Service initialized successfully");
        // Provide ButtonsProcessor to BLE config service for real-time button monitoring
        bleConfig.setButtonsProcessor(&buttons);
      } else {
        Serial.println("BLE Config Service initialization failed!");
      }
      
      // NOW start advertising (after all services are initialized)
      Serial.println("Starting BLE advertising...");
      bleHID.startAdvertising();
      Serial.println("Waiting for connection...");
      digitalWrite(STATUS_LED_PIN, LOW);   // Turn off status LED
      // Connection LED will be controlled by BLE connection status
    } else {
      Serial.println("BLE HID initialization failed!");
      blinkStatus(5);
    }
  }
  
  Serial.println("Setup complete!");
  Serial.println("=================================");
}

void loop() {
  // Handle BLE configuration mode
  if (currentMode == MODE_BLE) {
    handleBLEConfigMode();
  }
  
  // Check for serial commands
  if (Serial.available()) {
    handleSerialCommand();
  }
  
  // Update all inputs
  updateInputs();
  
  // Send HID reports at configured poll rate
  uint32_t now = millis();
  uint32_t reportInterval = deviceConfig.usbPollRate; // 1ms for USB, can be adjusted
  
  if (currentMode == MODE_BLE) {
    // For BLE, send reports at a reasonable rate (e.g., every 8ms for ~125Hz)
    reportInterval = 8;
  }
  
  if (now - lastReportTime >= reportInterval) {
    sendHIDReports();
    lastReportTime = now;
  }
  
  // Update battery level periodically (BLE only)
  if (currentMode == MODE_BLE && now - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    updateBatteryLevel();
    lastBatteryCheck = now;
  }
  
  // Update connection LED for BLE
  if (currentMode == MODE_BLE) {
    digitalWrite(CONNECTION_LED_PIN, bleHID.isConnected() ? HIGH : LOW);
    
    // Update axis monitoring for configurator (only when connected)
    if (bleHID.isConnected()) {
      bleConfig.updateAxisMonitoring();
    }
  }
  
  // Small delay to prevent tight looping (optional)
  // delayMicroseconds(100);
}

void selectOperationMode() {
#if DEV_FORCE_BLE_MODE
  // Development mode: Force BLE for testing
  currentMode = MODE_BLE;
  Serial.println(">>> DEVELOPMENT MODE: Forced BLE <<<");
  Serial.println("(Set DEV_FORCE_BLE_MODE to 0 to restore normal operation)");
  return;
#endif

  // Read mode select pin
  pinMode(MODE_SELECT_PIN, INPUT_PULLUP);
  delay(10); // Debounce
  
  bool modeSelectHigh = digitalRead(MODE_SELECT_PIN) == HIGH;
  
  // Check config mode setting
  if (deviceConfig.mode == MODE_AUTO) {
    // Auto mode: prefer USB if connected, otherwise BLE
    // For now, default to USB in AUTO mode
    currentMode = MODE_USB;
    Serial.println("AUTO mode: defaulting to USB");
  } else if (deviceConfig.mode == MODE_USB) {
    currentMode = MODE_USB;
    Serial.println("Config mode: USB");
  } else if (deviceConfig.mode == MODE_BLE) {
    currentMode = MODE_BLE;
    Serial.println("Config mode: BLE");
  }
  
  // Mode select pin override (for manual switching)
  // HIGH = USB, LOW = BLE
  if (modeSelectHigh) {
    Serial.println("Mode select pin: USB (override)");
    currentMode = MODE_USB;
  } else {
    Serial.println("Mode select pin: BLE (override)");
    currentMode = MODE_BLE;
  }
  
  Serial.print("Selected mode: ");
  Serial.println(currentMode == MODE_USB ? "USB" : "BLE");
}

void initializeHardware() {
  Serial.println("Initializing axes...");
  axes.begin(&deviceConfig);
  
  Serial.println("Initializing buttons...");
  buttons.begin(&deviceConfig);
  
  // Print pin configuration
  Serial.println("\nPin Configuration:");
  Serial.println("Axes:");
  for (int i = 0; i < MAX_AXES; i++) {
    if (deviceConfig.axes[i].enabled) {
      Serial.print("  Axis ");
      Serial.print(i);
      Serial.print(": Pin ");
      Serial.print(deviceConfig.axes[i].pin);
      Serial.print(" (");
      Serial.print(deviceConfig.axes[i].inverted ? "inverted" : "normal");
      Serial.println(")");
    }
  }
  
  Serial.println("Buttons:");
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (deviceConfig.buttons[i].enabled) {
      Serial.print("  Button ");
      Serial.print(i);
      Serial.print(": Pin ");
      Serial.print(deviceConfig.buttons[i].pin);
      Serial.print(" -> Logical ");
      Serial.println(deviceConfig.buttons[i].logicalNumber);
    }
  }
}

void updateInputs() {
  // Update axes (read ADC, apply calibration, curves)
  axes.update();
  
  // Update buttons (read pins, debounce, map)
  buttons.update();
}

void sendHIDReports() {
  // SAFETY: Don't send HID reports in configuration mode
  // This prevents floating pins from generating random input during initial setup
  if (configurationMode && currentMode == MODE_BLE) {
    return;
  }
  
  // Gather axis values
  int16_t axisValues[MAX_AXES];
  for (int i = 0; i < MAX_AXES; i++) {
    axisValues[i] = axes.getAxisValue(i);
  }
  
  // Gather button states
  uint16_t buttonBitmask = buttons.getButtonBitmask();
  
  // Send via appropriate interface
  if (currentMode == MODE_USB) {
    usbHID.sendReport(axisValues, buttonBitmask);
  } else if (currentMode == MODE_BLE) {
    bleHID.sendReport(axisValues, buttonBitmask);
  }
}

void updateBatteryLevel() {
  float vbat = readVBAT();
  
  // Convert voltage to percentage (rough estimate)
  // LiPo: 4.2V = 100%, 3.7V = 50%, 3.0V = 0%
  uint8_t batteryPercent = 100;
  
  if (vbat >= 4.2) {
    batteryPercent = 100;
  } else if (vbat <= 3.0) {
    batteryPercent = 0;
  } else {
    // Linear approximation
    batteryPercent = (uint8_t)((vbat - 3.0) / (4.2 - 3.0) * 100.0);
  }
  
  Serial.print("Battery: ");
  Serial.print(vbat);
  Serial.print("V (");
  Serial.print(batteryPercent);
  Serial.println("%)");
  
  // Update BLE battery service (if BLE is being used)
  // Note: This requires access to BLEBas, which could be exposed via bleHID
  // For now, this is a placeholder
}

float readVBAT() {
  // Read battery voltage via analog pin
  float raw = analogRead(VBAT_PIN);
  
  // Convert to voltage
  float vbat = raw * VBAT_MV_PER_LSB * VBAT_DIVIDER_COMP / 1000.0;
  
  return vbat;
}

void blinkStatus(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(200);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(200);
  }
}

void handleBLEConfigMode() {
  bool isConnected = bleHID.isConnected();
  
  // Detect new connection
  if (isConnected && !wasConnected) {
    bleConnectionTime = millis();
    configurationMode = true;
    bleHID.setHIDEnabled(false);
    Serial.println("=======================================");
    Serial.println("BLE CONNECTED - Configuration Mode Active");
    Serial.println("HID reports disabled for 60 seconds");
    Serial.println("");
    Serial.println("** SAFETY MODE: All inputs disabled! **");
    Serial.println("Enable only connected axes/buttons in");
    Serial.println("the configurator to prevent noise.");
    Serial.println("");
    Serial.println("Hold MODE button or send 'C' to stay in config mode");
    Serial.println("Send 'H' to enable HID mode immediately");
    Serial.println("=======================================");
  }
  
  // Detect disconnection
  if (!isConnected && wasConnected) {
    configurationMode = false;
    bleHID.setHIDEnabled(false);
    Serial.println("BLE disconnected");
  }
  
  wasConnected = isConnected;
  
  // Auto-enable HID after timeout if still in config mode
  if (configurationMode && isConnected) {
    // Check if mode button is held (stay in config mode)
    bool modeButtonHeld = digitalRead(MODE_SELECT_PIN) == LOW;
    
    uint32_t elapsedTime = millis() - bleConnectionTime;
    
    if (modeButtonHeld) {
      // Reset timer while button is held
      bleConnectionTime = millis();
      Serial.println("Mode button held - staying in config mode");
    } else if (elapsedTime >= CONFIG_MODE_TIMEOUT) {
      // Timeout reached, enable HID
      configurationMode = false;
      bleHID.setHIDEnabled(true);
      Serial.println("Config mode timeout - HID enabled");
    }
  }
}

void handleSerialCommand() {
  char cmd = Serial.read();
  
  switch (cmd) {
    case 'C':
    case 'c':
      // Enter configuration mode
      if (currentMode == MODE_BLE) {
        configurationMode = true;
        bleHID.setHIDEnabled(false);
        bleConnectionTime = millis(); // Reset timer
        Serial.println("Configuration mode enabled (HID disabled)");
      } else {
        Serial.println("Configuration mode only available in BLE mode");
      }
      break;
      
    case 'H':
    case 'h':
      // Enable HID mode
      if (currentMode == MODE_BLE) {
        configurationMode = false;
        bleHID.setHIDEnabled(true);
        Serial.println("HID mode enabled");
      } else {
        Serial.println("Already in USB HID mode");
      }
      break;
      
    case '?':
      // Print help
      Serial.println("\n=== SeedJoy Commands ===");
      Serial.println("C - Enter configuration mode (disable HID)");
      Serial.println("H - Enable HID mode");
      Serial.println("? - Show this help");
      Serial.println("=======================\n");
      break;
  }
}
