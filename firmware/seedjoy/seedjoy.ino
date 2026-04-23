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
#define DEV_FORCE_BLE_MODE 0 // Set to 1 to force BLE mode, 0 for normal operation

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
bool serialButtonStream = false;   // true while browser is on the Button Mapping tab
uint32_t lastButtonStreamTime = 0; // rate-limit to ~20 Hz

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
    Serial.println("========================================");
    Serial.println("       INITIALIZING BLE MODE");
    Serial.println("========================================");
    
    if (bleHID.begin(&deviceConfig)) {
      Serial.println("BLE HID initialized successfully");
      
      // Initialize BLE config service BEFORE starting advertising
      Serial.println("");
      Serial.println("Starting BLE Config Service...");
      if (bleConfig.begin(&deviceConfig, &storage, &axes)) {
        Serial.println("BLE Config Service initialized successfully");
        // Provide ButtonsProcessor to BLE config service for real-time button monitoring
        bleConfig.setButtonsProcessor(&buttons);
      } else {
        Serial.println("BLE Config Service initialization failed!");
      }
      
      // NOW start advertising (after all services are initialized)
      // Pass config service so it's included in advertising packet for WebBluetooth discovery
      Serial.println("");
      Serial.println("Starting BLE advertising...");
      bleHID.startAdvertising(&bleConfig.getService());
      Serial.println("Waiting for connection...");
      Serial.println("========================================");
      Serial.println("");
      digitalWrite(STATUS_LED_PIN, LOW);   // Turn off status LED
      // Connection LED will be controlled by BLE connection status
    } else {
      Serial.println("BLE HID initialization failed!");
      blinkStatus(5);
    }
  }
  
  Serial.println("Setup complete!");
  Serial.println("========================================");
  Serial.println("");
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
  
  // Stream button states over Serial for the Web Serial button-test tab (~20 Hz)
  if (serialButtonStream && (now - lastButtonStreamTime >= 50)) {
    uint8_t bBytes[8];
    buttons.getExtendedButtonStates(bBytes, 8);
    Serial.print("{\"type\":\"buttons\",\"data\":{\"states\":[");
    for (int bi = 0; bi < 8; bi++) {
      if (bi > 0) Serial.print(",");
      Serial.print(bBytes[bi]);
    }
    Serial.println("]}}");
    lastButtonStreamTime = now;
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
  
  Serial.println("Buttons (GPIO):");
  bool anyGpio = false;
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (deviceConfig.buttons[i].enabled) {
      anyGpio = true;
      Serial.print("  Button ");
      Serial.print(i);
      Serial.print(": Pin ");
      Serial.print(deviceConfig.buttons[i].pin);
      Serial.print(" -> Logical ");
      Serial.println(deviceConfig.buttons[i].logicalNumber);
    }
  }
  if (!anyGpio) Serial.println("  (none enabled - SR-only mode)");

  Serial.println("Shift Registers:");
  if (deviceConfig.shiftRegisters.enabled) {
    Serial.print("  Chips  : "); Serial.println(deviceConfig.shiftRegisters.numChips);
    Serial.print("  Buttons: "); Serial.println(deviceConfig.shiftRegisters.numChips * 8);
    Serial.print("  Data   : D"); Serial.println(deviceConfig.shiftRegisters.dataPin);
    Serial.print("  Clock  : D"); Serial.println(deviceConfig.shiftRegisters.clockPin);
    Serial.print("  Load   : D"); Serial.println(deviceConfig.shiftRegisters.loadPin);
    Serial.print("  Inverted: "); Serial.println(deviceConfig.shiftRegisters.inverted ? "yes" : "no");
  } else {
    Serial.println("  (disabled)");
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
  
  // Gather button states (8 bytes = 64 buttons)
  uint8_t buttonBytes[8];
  buttons.getExtendedButtonStates(buttonBytes, 8);
  
  // Send via appropriate interface
  if (currentMode == MODE_USB) {
    usbHID.sendReport(axisValues, buttonBytes);
  } else if (currentMode == MODE_BLE) {
    bleHID.sendReport(axisValues, buttonBytes);
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
      // Only log once per second to avoid flooding
      static uint32_t lastButtonLog = 0;
      if (millis() - lastButtonLog > 1000) {
        Serial.println("Mode button held - staying in config mode");
        lastButtonLog = millis();
      }
    } else if (elapsedTime >= CONFIG_MODE_TIMEOUT) {
      // Re-init SR hardware before enabling HID — prevents ghost inputs if a
      // BLE config write changed SR pin assignments or chip count during the
      // config window.
      buttons.begin(&deviceConfig);
      configurationMode = false;
      bleHID.setHIDEnabled(true);
      Serial.println("Config mode timeout - HID enabled");
    }
  }
}

void handleSerialCommand() {
  // Read available characters into a buffer
  static String commandBuffer = "";
  
  while (Serial.available()) {
    char c = Serial.read();
    
    // Line-based command (ends with newline)
    if (c == '\n' || c == '\r') {
      if (commandBuffer.length() > 0) {
        processCommand(commandBuffer);
        commandBuffer = "";
      }
      return;
    }
    
    commandBuffer += c;
    
    // Single-char commands (backwards compatibility)
    if (commandBuffer.length() == 1) {
      char cmd = commandBuffer[0];
      
      if (cmd == 'C' || cmd == 'c' || cmd == 'H' || cmd == 'h' || cmd == '?') {
        processCommand(commandBuffer);
        commandBuffer = "";
        return;
      }
    }
  }
}

void processCommand(String cmd) {
  cmd.trim();
  
  // Single character commands
  if (cmd.length() == 1) {
    char c = cmd[0];
    
    switch (c) {
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
          // Re-init SR hardware before enabling HID to flush any stale pin/chip state
          buttons.begin(&deviceConfig);
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
        Serial.println("read_config - Send config as JSON");
        Serial.println("write_config:{JSON} - Write config from JSON");
        Serial.println("ping - Test connection");
        Serial.println("? - Show this help");
        Serial.println("=======================\n");
        break;
    }
    return;
  }
  
  // Multi-character commands
  if (cmd.startsWith("read_config")) {
    sendConfigAsJSON();
  }
  else if (cmd.startsWith("write_config:")) {
    String jsonData = cmd.substring(13); // Skip "write_config:"
    receiveConfigFromJSON(jsonData);
  }
  else if (cmd == "ping") {
    Serial.println("{\"type\":\"pong\"}");
  }
  else if (cmd == "stream_buttons") {
    serialButtonStream = true;
    Serial.println("{\"type\":\"status\",\"message\":\"Button stream started\",\"success\":true}");
  }
  else if (cmd == "stop_stream") {
    serialButtonStream = false;
    Serial.println("{\"type\":\"status\",\"message\":\"Button stream stopped\",\"success\":true}");
  }
  else {
    Serial.print("Unknown command: ");
    Serial.println(cmd);
  }
}

// ─── JSON helpers (no external library needed) ───────────────────────────────
// Extract an integer value for a key from a flat JSON string.
// Returns defaultVal if key not found.
int extractJsonInt(const String& json, const String& key, int defaultVal = 0) {
  String search = "\"" + key + "\":";  // e.g. "numChips":
  int idx = json.indexOf(search);
  if (idx < 0) return defaultVal;
  idx += search.length();
  // Skip whitespace
  while (idx < (int)json.length() && json[idx] == ' ') idx++;
  // Read digits (and optional leading minus)
  String num = "";
  if (idx < (int)json.length() && json[idx] == '-') { num += '-'; idx++; }
  while (idx < (int)json.length() && isdigit(json[idx])) { num += json[idx++]; }
  return num.length() ? num.toInt() : defaultVal;
}

// Extract a boolean value for a key. Returns defaultVal if key not found.
bool extractJsonBool(const String& json, const String& key, bool defaultVal = false) {
  String search = "\"" + key + "\":";  // e.g. "enabled":
  int idx = json.indexOf(search);
  if (idx < 0) return defaultVal;
  idx += search.length();
  while (idx < (int)json.length() && json[idx] == ' ') idx++;
  if (idx + 3 < (int)json.length() && json.substring(idx, idx + 4) == "true")  return true;
  if (idx + 4 < (int)json.length() && json.substring(idx, idx + 5) == "false") return false;
  return defaultVal;
}

// Extract a string value for a key (returns content between the quotes).
// Returns defaultVal if key not found.
String extractJsonStr(const String& json, const String& key, const String& defaultVal = "") {
  String search = "\"" + key + "\":\"";
  int idx = json.indexOf(search);
  if (idx < 0) return defaultVal;
  idx += search.length();
  int end = json.indexOf('"', idx);
  if (end < 0) return defaultVal;
  return json.substring(idx, end);
}
// ─────────────────────────────────────────────────────────────────────────────
float extractJsonFloat(const String& json, const String& key, float defaultVal = 0.0f) {
  String search = "\"" + key + "\":";
  int idx = json.indexOf(search);
  if (idx < 0) return defaultVal;
  idx += search.length();
  while (idx < (int)json.length() && json[idx] == ' ') idx++;
  String num = "";
  if (idx < (int)json.length() && json[idx] == '-') { num += '-'; idx++; }
  while (idx < (int)json.length() && (isdigit(json[idx]) || json[idx] == '.')) { num += json[idx++]; }
  return num.length() ? num.toFloat() : defaultVal;
}
void sendConfigAsJSON() {
  Serial.print("{\"type\":\"config\",\"data\":{");

  // Device settings
  Serial.print("\"deviceName\":\"");   Serial.print(deviceConfig.deviceName);  Serial.print("\",");
  Serial.print("\"mode\":");           Serial.print(deviceConfig.mode);          Serial.print(",");
  Serial.print("\"usbPollRate\":");    Serial.print(deviceConfig.usbPollRate);   Serial.print(",");
  Serial.print("\"bleConnInterval\":"); Serial.print(deviceConfig.bleConnInterval); Serial.print(",");
  Serial.print("\"bleTxPower\":");     Serial.print(deviceConfig.bleTxPower);    Serial.print(",");
  Serial.print("\"autoSleep\":");      Serial.print(deviceConfig.autoSleep ? "true" : "false"); Serial.print(",");
  Serial.print("\"sleepTimeout\":");   Serial.print(deviceConfig.sleepTimeout);  Serial.print(",");

  // Shift register config
  Serial.print("\"shiftRegisters\":{");
  Serial.print("\"enabled\":");  Serial.print(deviceConfig.shiftRegisters.enabled  ? "true" : "false"); Serial.print(",");
  Serial.print("\"numChips\":"); Serial.print(deviceConfig.shiftRegisters.numChips); Serial.print(",");
  Serial.print("\"dataPin\":");  Serial.print(deviceConfig.shiftRegisters.dataPin);  Serial.print(",");
  Serial.print("\"clockPin\":"); Serial.print(deviceConfig.shiftRegisters.clockPin); Serial.print(",");
  Serial.print("\"loadPin\":");  Serial.print(deviceConfig.shiftRegisters.loadPin);  Serial.print(",");
  Serial.print("\"inverted\":"); Serial.print(deviceConfig.shiftRegisters.inverted  ? "true" : "false");
  Serial.print("},");

  // Axes
  Serial.print("\"axes\":[");
  for (int i = 0; i < MAX_AXES; i++) {
    if (i > 0) Serial.print(",");
    Serial.print("{");
    Serial.print("\"enabled\":");   Serial.print(deviceConfig.axes[i].enabled   ? "true" : "false"); Serial.print(",");
    Serial.print("\"pin\":");       Serial.print(deviceConfig.axes[i].pin);       Serial.print(",");
    Serial.print("\"min\":");       Serial.print(deviceConfig.axes[i].min);       Serial.print(",");
    Serial.print("\"center\":");    Serial.print(deviceConfig.axes[i].center);    Serial.print(",");
    Serial.print("\"max\":");       Serial.print(deviceConfig.axes[i].max);       Serial.print(",");
    Serial.print("\"deadzone\":");  Serial.print(deviceConfig.axes[i].deadzone);  Serial.print(",");
    Serial.print("\"curveType\":"); Serial.print(deviceConfig.axes[i].curveType); Serial.print(",");
    Serial.print("\"expoFactor\":"); Serial.print(deviceConfig.axes[i].expoFactor, 2); Serial.print(",");
    Serial.print("\"inverted\":");  Serial.print(deviceConfig.axes[i].inverted   ? "true" : "false"); Serial.print(",");
    Serial.print("\"smoothing\":"); Serial.print(deviceConfig.axes[i].smoothing);
    Serial.print("}");
  }
  Serial.print("],");

  // Buttons
  Serial.print("\"buttons\":[");
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (i > 0) Serial.print(",");
    Serial.print("{");
    Serial.print("\"enabled\":");      Serial.print(deviceConfig.buttons[i].enabled      ? "true" : "false"); Serial.print(",");
    Serial.print("\"pin\":");           Serial.print(deviceConfig.buttons[i].pin);           Serial.print(",");
    Serial.print("\"logicalNumber\":"); Serial.print(deviceConfig.buttons[i].logicalNumber); Serial.print(",");
    Serial.print("\"inverted\":");      Serial.print(deviceConfig.buttons[i].inverted      ? "true" : "false");
    Serial.print("}");
  }
  Serial.println("]}}");
}

void receiveConfigFromJSON(String json) {
  // Device settings
  String name = extractJsonStr(json, "deviceName");
  if (name.length() > 0) {
    name = name.substring(0, 31);  // Enforce max length
    strncpy(deviceConfig.deviceName, name.c_str(), sizeof(deviceConfig.deviceName) - 1);
    deviceConfig.deviceName[sizeof(deviceConfig.deviceName) - 1] = '\0';
  }

  int modeVal = extractJsonInt(json, "mode", -1);
  if (modeVal >= 0 && modeVal <= 2) deviceConfig.mode = (OperationMode)modeVal;

  int pollRate = extractJsonInt(json, "usbPollRate", -1);
  if (pollRate > 0) deviceConfig.usbPollRate = (uint8_t)pollRate;

  int bleInterval = extractJsonInt(json, "bleConnInterval", -1);
  if (bleInterval > 0) deviceConfig.bleConnInterval = (uint16_t)bleInterval;

  int txPower = extractJsonInt(json, "bleTxPower", -99);
  if (txPower != -99) deviceConfig.bleTxPower = (int8_t)txPower;

  // Power management
  if (json.indexOf("\"autoSleep\":") >= 0)
    deviceConfig.autoSleep = extractJsonBool(json, "autoSleep");
  int sleepTimeout = extractJsonInt(json, "sleepTimeout", -1);
  if (sleepTimeout > 0) deviceConfig.sleepTimeout = (uint16_t)sleepTimeout;

  // Shift registers — locate the shiftRegisters object
  int srIdx = json.indexOf("\"shiftRegisters\":{");
  if (srIdx >= 0) {
    // Extract the SR sub-object
    int srOpen  = json.indexOf('{', srIdx + 17);
    int srClose = json.indexOf('}', srOpen);
    if (srOpen >= 0 && srClose > srOpen) {
      String srJson = json.substring(srOpen, srClose + 1);
      if (srJson.indexOf("\"enabled\":") >= 0)
        deviceConfig.shiftRegisters.enabled  = extractJsonBool(srJson, "enabled");
      int nc = extractJsonInt(srJson, "numChips", -1);
      if (nc >= 1 && nc <= 16) deviceConfig.shiftRegisters.numChips = (uint8_t)nc;
      int dp = extractJsonInt(srJson, "dataPin",  -1);
      if (dp >= 0)  deviceConfig.shiftRegisters.dataPin  = (uint8_t)dp;
      int cp = extractJsonInt(srJson, "clockPin", -1);
      if (cp >= 0)  deviceConfig.shiftRegisters.clockPin = (uint8_t)cp;
      int lp = extractJsonInt(srJson, "loadPin",  -1);
      if (lp >= 0)  deviceConfig.shiftRegisters.loadPin  = (uint8_t)lp;
      if (srJson.indexOf("\"inverted\":") >= 0)
        deviceConfig.shiftRegisters.inverted = extractJsonBool(srJson, "inverted");
    }
  }

  // Axes
  int axesStart = json.indexOf("\"axes\":[");
  if (axesStart >= 0) {
    int arrOpen = json.indexOf('[', axesStart + 6);
    int pos = arrOpen + 1;
    for (int i = 0; i < MAX_AXES; i++) {
      int objOpen  = json.indexOf('{', pos);
      if (objOpen  < 0) break;
      int objClose = json.indexOf('}', objOpen);
      if (objClose < 0) break;
      String axJson = json.substring(objOpen, objClose + 1);
      if (axJson.indexOf("\"enabled\":") >= 0)
        deviceConfig.axes[i].enabled = extractJsonBool(axJson, "enabled");
      int pin = extractJsonInt(axJson, "pin", -1);
      if (pin >= 0) deviceConfig.axes[i].pin = (uint8_t)pin;
      int mn = extractJsonInt(axJson, "min", -1);
      if (mn >= 0) deviceConfig.axes[i].min = (uint16_t)mn;
      int ctr = extractJsonInt(axJson, "center", -1);
      if (ctr >= 0) deviceConfig.axes[i].center = (uint16_t)ctr;
      int mx = extractJsonInt(axJson, "max", -1);
      if (mx >= 0) deviceConfig.axes[i].max = (uint16_t)mx;
      int dz = extractJsonInt(axJson, "deadzone", -1);
      if (dz >= 0) deviceConfig.axes[i].deadzone = (uint8_t)dz;
      int ct = extractJsonInt(axJson, "curveType", -1);
      if (ct >= 0 && ct <= 2) deviceConfig.axes[i].curveType = (AxisCurve)ct;
      float ef = extractJsonFloat(axJson, "expoFactor", -1.0f);
      if (ef >= 0.0f) deviceConfig.axes[i].expoFactor = ef;
      if (axJson.indexOf("\"inverted\":") >= 0)
        deviceConfig.axes[i].inverted = extractJsonBool(axJson, "inverted");
      int sm = extractJsonInt(axJson, "smoothing", -1);
      if (sm >= 0) deviceConfig.axes[i].smoothing = (uint8_t)sm;
      pos = objClose + 1;
    }
  }

  // Buttons
  int btnsStart = json.indexOf("\"buttons\":[");
  if (btnsStart >= 0) {
    int arrOpen = json.indexOf('[', btnsStart + 9);
    int pos = arrOpen + 1;
    for (int i = 0; i < MAX_BUTTONS; i++) {
      int objOpen  = json.indexOf('{', pos);
      if (objOpen  < 0) break;
      int objClose = json.indexOf('}', objOpen);
      if (objClose < 0) break;
      String btnJson = json.substring(objOpen, objClose + 1);
      if (btnJson.indexOf("\"enabled\":") >= 0)
        deviceConfig.buttons[i].enabled = extractJsonBool(btnJson, "enabled");
      int pin = extractJsonInt(btnJson, "pin", -1);
      if (pin >= 0) deviceConfig.buttons[i].pin = (uint8_t)pin;
      int ln = extractJsonInt(btnJson, "logicalNumber", -1);
      if (ln >= 0) deviceConfig.buttons[i].logicalNumber = (uint8_t)ln;
      if (btnJson.indexOf("\"inverted\":") >= 0)
        deviceConfig.buttons[i].inverted = extractJsonBool(btnJson, "inverted");
      pos = objClose + 1;
    }
  }

  // Save to flash
  if (storage.saveConfig(&deviceConfig)) {
    // Full re-init: recalculates SR pins, srOffset_, totalButtonCount_
    buttons.begin(&deviceConfig);
    axes.setConfig(&deviceConfig);
    Serial.println("{\"type\":\"status\",\"message\":\"Config saved\",\"success\":true}");
    Serial.print("SR: enabled=");
    Serial.print(deviceConfig.shiftRegisters.enabled ? "true" : "false");
    Serial.print(" chips=");
    Serial.print(deviceConfig.shiftRegisters.numChips);
    Serial.print(" buttons=");
    Serial.println(deviceConfig.shiftRegisters.numChips * 8);
  } else {
    Serial.println("{\"type\":\"status\",\"message\":\"Failed to save config\",\"success\":false}");
  }
}
