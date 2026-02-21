/**
 * SeedJoy - BLE Configuration Service Implementation
 */

#include "ble_config.h"
#include <Arduino.h>

// Static instance pointer for callbacks
BLEConfigService* BLEConfigService::instance_ = nullptr;

BLEConfigService::BLEConfigService() 
  : service_(BLE_CONFIG_SERVICE_UUID),
    configReadChar_(BLE_CONFIG_READ_UUID),
    configWriteChar_(BLE_CONFIG_WRITE_UUID),
    statusChar_(BLE_STATUS_UUID),
    axesMonitorChar_(BLE_AXES_MONITOR_UUID),
    buttonsMonitorChar_(BLE_BUTTONS_MONITOR_UUID),
    calibrateChar_(BLE_CALIBRATE_UUID),
    config_(nullptr),
    storage_(nullptr),
    axes_(nullptr),
    buttons_(nullptr),
    configBufferSize_(0) {
  
  instance_ = this;
  memset(&axisData_, 0, sizeof(axisData_));
  memset(&buttonsData_, 0, sizeof(buttonsData_));
}

bool BLEConfigService::begin(DeviceConfig* config, StorageManager* storage, AxesProcessor* axes) {
  config_ = config;
  storage_ = storage;
  axes_ = axes;
  
  Serial.println("Configuring BLE Config Service characteristics...");
  
  // Configure ALL characteristics BEFORE starting the service
  // (Bluefruit requires characteristics to be added before service.begin())
  
  // Configure Config Read characteristic (Read only, notify)
  configReadChar_.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  configReadChar_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  configReadChar_.setMaxLen(MAX_CONFIG_SIZE);
  configReadChar_.setFixedLen(false);
  uint16_t err1 = configReadChar_.begin();
  Serial.print("Config Read Char initialized: ");
  Serial.println(err1 == ERROR_NONE ? "SUCCESS" : "FAILED");
  
  // Configure Config Write characteristic (Write only)
  configWriteChar_.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
  configWriteChar_.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  configWriteChar_.setMaxLen(MAX_CONFIG_SIZE);
  configWriteChar_.setFixedLen(false);
  configWriteChar_.setWriteCallback(configWriteCallback);
  uint16_t err2 = configWriteChar_.begin();
  Serial.print("Config Write Char initialized: ");
  Serial.println(err2 == ERROR_NONE ? "SUCCESS" : "FAILED");
  
  // Configure Status characteristic (Read, notify)
  statusChar_.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  statusChar_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  statusChar_.setMaxLen(256);
  statusChar_.setFixedLen(false);
  statusChar_.begin();
  
  // Configure Axes Monitor characteristic (Read, notify)
  axesMonitorChar_.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  axesMonitorChar_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  axesMonitorChar_.setMaxLen(sizeof(AxisMonitorData));
  axesMonitorChar_.setFixedLen(true);
  axesMonitorChar_.begin();
  
  // Configure Buttons Monitor characteristic (Read, notify)
  buttonsMonitorChar_.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  buttonsMonitorChar_.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  buttonsMonitorChar_.setMaxLen(sizeof(ButtonsMonitorData));
  buttonsMonitorChar_.setFixedLen(true);
  buttonsMonitorChar_.begin();
  
  // Configure Calibrate characteristic (Write)
  calibrateChar_.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
  calibrateChar_.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
  calibrateChar_.setMaxLen(16);
  calibrateChar_.setFixedLen(false);
  calibrateChar_.setWriteCallback(calibrateCallback);
  calibrateChar_.begin();
  
  // NOW start the service (adds all characteristics to the service)
  service_.begin();
  Serial.print("BLE Config Service started with UUID: ");
  Serial.println(BLE_CONFIG_SERVICE_UUID);
  
  // Serialize initial config
  serializeConfig();
  
  // Send initial status
  sendStatus("SeedJoy Config Service Ready", true);
  
  Serial.println("BLE Config Service initialized");
  
  return true;
}

void BLEConfigService::updateAxisMonitoring() {
  if (!axes_ || !Bluefruit.connected()) return;
  
  // Update axis data
  axisData_.timestamp = millis();
  for (int i = 0; i < MAX_AXES; i++) {
    axisData_.processed[i] = axes_->getAxisValue(i);
    axisData_.raw[i] = axes_->getRawValue(i);
  }
  
  // Notify subscribers if any
  if (axesMonitorChar_.notifyEnabled()) {
    axesMonitorChar_.notify(&axisData_, sizeof(axisData_));
  }

  // Buttons monitoring
  if (buttons_ && buttonsMonitorChar_.notifyEnabled()) {
    buttons_->getExtendedButtonStates(buttonsData_.states, 8);
    buttonsData_.timestamp = millis();
    buttonsMonitorChar_.notify(&buttonsData_, sizeof(buttonsData_));
  }
}

void BLEConfigService::setAxesProcessor(AxesProcessor* axes) {
  axes_ = axes;
}

void BLEConfigService::setButtonsProcessor(ButtonsProcessor* buttons) {
  buttons_ = buttons;
}

BLEService& BLEConfigService::getService() {
  return service_;
}

bool BLEConfigService::serializeConfig() {
  if (!config_) return false;
  
  // Use ArduinoJson to serialize config
  // For simplicity, we'll use a basic JSON string builder
  String json = "{";
  
  // Device info
  json += "\"deviceName\":\"" + String(config_->deviceName) + "\",";
  json += "\"mode\":" + String(config_->mode) + ",";
  json += "\"usbPollRate\":" + String(config_->usbPollRate) + ",";
  json += "\"bleTxPower\":" + String(config_->bleTxPower) + ",";
  json += "\"bleConnInterval\":" + String(config_->bleConnInterval) + ",";
  
  // Axes
  json += "\"axes\":[";
  for (int i = 0; i < MAX_AXES; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"enabled\":" + String(config_->axes[i].enabled ? "true" : "false") + ",";
    json += "\"pin\":" + String(config_->axes[i].pin) + ",";
    json += "\"min\":" + String(config_->axes[i].min) + ",";
    json += "\"center\":" + String(config_->axes[i].center) + ",";
    json += "\"max\":" + String(config_->axes[i].max) + ",";
    json += "\"deadzone\":" + String(config_->axes[i].deadzone) + ",";
    json += "\"curveType\":" + String(config_->axes[i].curveType) + ",";
    json += "\"expoFactor\":" + String(config_->axes[i].expoFactor, 2) + ",";
    json += "\"inverted\":" + String(config_->axes[i].inverted ? "true" : "false") + ",";
    json += "\"smoothing\":" + String(config_->axes[i].smoothing);
    json += "}";
  }
  json += "],";
  
// Shift register config
  json += "\"shiftRegisters\":{";
  json += "\"enabled\":"  + String(config_->shiftRegisters.enabled  ? "true" : "false") + ",";
  json += "\"numChips\":" + String(config_->shiftRegisters.numChips) + ",";
  json += "\"dataPin\":"  + String(config_->shiftRegisters.dataPin)  + ",";
  json += "\"clockPin\":" + String(config_->shiftRegisters.clockPin) + ",";
  json += "\"loadPin\":"  + String(config_->shiftRegisters.loadPin)  + ",";
  json += "\"inverted\":" + String(config_->shiftRegisters.inverted ? "true" : "false");
  json += "},";

  // Buttons (GPIO - just enabled/pin/logical)
  json += "\"buttons\":["; 
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"enabled\":"      + String(config_->buttons[i].enabled      ? "true" : "false") + ",";
    json += "\"pin\":"           + String(config_->buttons[i].pin)           + ",";
    json += "\"logicalNumber\":" + String(config_->buttons[i].logicalNumber) + ",";
    json += "\"inverted\":"      + String(config_->buttons[i].inverted      ? "true" : "false");
    json += "}";
  }
  json += "]";
  
  json += "}";
  
  // Copy to buffer
  configBufferSize_ = json.length();
  if (configBufferSize_ > MAX_CONFIG_SIZE) {
    Serial.println("ERROR: Config too large for buffer!");
    return false;
  }
  
  memcpy(configBuffer_, json.c_str(), configBufferSize_);
  
  // Update characteristic
  configReadChar_.write(configBuffer_, configBufferSize_);
  
  Serial.print("Config serialized: ");
  Serial.print(configBufferSize_);
  Serial.println(" bytes");
  
  return true;
}

// ─── JSON helpers (mirrors seedjoy.ino) ─────────────────────────────────────
static int bleExtractJsonInt(const String& j, const String& key, int dv = 0) {
  String s = "\"" + key + "\":";
  int idx = j.indexOf(s);
  if (idx < 0) return dv;
  idx += s.length();
  while (idx < (int)j.length() && j[idx] == ' ') idx++;
  String n = "";
  if (idx < (int)j.length() && j[idx] == '-') { n += '-'; idx++; }
  while (idx < (int)j.length() && isdigit(j[idx])) n += j[idx++];
  return n.length() ? n.toInt() : dv;
}
static bool bleExtractJsonBool(const String& j, const String& key, bool dv = false) {
  String s = "\"" + key + "\":";
  int idx = j.indexOf(s);
  if (idx < 0) return dv;
  idx += s.length();
  while (idx < (int)j.length() && j[idx] == ' ') idx++;
  if (idx + 3 < (int)j.length() && j.substring(idx, idx + 4) == "true")  return true;
  if (idx + 4 < (int)j.length() && j.substring(idx, idx + 5) == "false") return false;
  return dv;
}
static String bleExtractJsonStr(const String& j, const String& key, const String& dv = "") {
  String s = "\"" + key + "\":\"";
  int idx = j.indexOf(s);
  if (idx < 0) return dv;
  idx += s.length();
  int end = j.indexOf('"', idx);
  return end < 0 ? dv : j.substring(idx, end);
}
// ─────────────────────────────────────────────────────────────────────────────

bool BLEConfigService::deserializeConfig(const uint8_t* data, uint16_t len) {
  if (!config_ || !data || len == 0) return false;

  if (len > MAX_CONFIG_SIZE) len = MAX_CONFIG_SIZE;
  char buf[MAX_CONFIG_SIZE + 1];
  memcpy(buf, data, len);
  buf[len] = '\0';
  String json = String(buf);

  Serial.print("Deserializing config ("); Serial.print(len); Serial.println(" bytes)");

  // Device name
  String name = bleExtractJsonStr(json, "deviceName");
  if (name.length() > 0) {
    name = name.substring(0, 31);
    strncpy(config_->deviceName, name.c_str(), sizeof(config_->deviceName) - 1);
    config_->deviceName[sizeof(config_->deviceName) - 1] = '\0';
  }

  int modeVal = bleExtractJsonInt(json, "mode", -1);
  if (modeVal >= 0 && modeVal <= 2) config_->mode = (OperationMode)modeVal;

  int pollRate = bleExtractJsonInt(json, "usbPollRate", -1);
  if (pollRate > 0) config_->usbPollRate = (uint8_t)pollRate;

  int bleInterval = bleExtractJsonInt(json, "bleConnInterval", -1);
  if (bleInterval > 0) config_->bleConnInterval = (uint16_t)bleInterval;

  int txPower = bleExtractJsonInt(json, "bleTxPower", -99);
  if (txPower != -99) config_->bleTxPower = (int8_t)txPower;

  if (json.indexOf("\"autoSleep\":") >= 0)
    config_->autoSleep = bleExtractJsonBool(json, "autoSleep");
  int sleepTimeout = bleExtractJsonInt(json, "sleepTimeout", -1);
  if (sleepTimeout > 0) config_->sleepTimeout = (uint16_t)sleepTimeout;

  // Shift registers
  int srIdx = json.indexOf("\"shiftRegisters\":{");
  if (srIdx >= 0) {
    int srOpen  = json.indexOf('{', srIdx + 17);
    int srClose = json.indexOf('}', srOpen);
    if (srOpen >= 0 && srClose > srOpen) {
      String srJson = json.substring(srOpen, srClose + 1);
      if (srJson.indexOf("\"enabled\":") >= 0)
        config_->shiftRegisters.enabled  = bleExtractJsonBool(srJson, "enabled");
      int nc = bleExtractJsonInt(srJson, "numChips", -1);
      if (nc >= 1 && nc <= 16) config_->shiftRegisters.numChips = (uint8_t)nc;
      int dp = bleExtractJsonInt(srJson, "dataPin",  -1);
      if (dp >= 0) config_->shiftRegisters.dataPin  = (uint8_t)dp;
      int cp = bleExtractJsonInt(srJson, "clockPin", -1);
      if (cp >= 0) config_->shiftRegisters.clockPin = (uint8_t)cp;
      int lp = bleExtractJsonInt(srJson, "loadPin",  -1);
      if (lp >= 0) config_->shiftRegisters.loadPin  = (uint8_t)lp;
      if (srJson.indexOf("\"inverted\":") >= 0)
        config_->shiftRegisters.inverted = bleExtractJsonBool(srJson, "inverted");
    }
  }

  // Full hardware re-init: recalculates SR pins, srOffset_, totalButtonCount_.
  // Must use begin() not setConfig() — critical when SR pin assignments or
  // numChips changed via BLE write.
  if (axes_)    axes_->setConfig(config_);
  if (buttons_) buttons_->begin(config_);

  Serial.print("SR: enabled=");
  Serial.print(config_->shiftRegisters.enabled ? "true" : "false");
  Serial.print(" chips="); Serial.print(config_->shiftRegisters.numChips);
  Serial.print(" buttons="); Serial.println(config_->shiftRegisters.numChips * 8);

  return true;
}

void BLEConfigService::sendStatus(const char* message, bool success) {
  if (!message) return;
  
  String statusJson = "{\"success\":" + String(success ? "true" : "false") + 
                      ",\"message\":\"" + String(message) + 
                      "\",\"fwVersion\":\"" + 
                      String(FIRMWARE_VERSION_MAJOR) + "." +
                      String(FIRMWARE_VERSION_MINOR) + "." +
                      String(FIRMWARE_VERSION_PATCH) + "\"}";
  
  statusChar_.write(statusJson.c_str());
  
  // Notify if enabled
  if (Bluefruit.connected() && statusChar_.notifyEnabled()) {
    statusChar_.notify(statusJson.c_str());
  }
}

void BLEConfigService::configWriteCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  if (!instance_) return;
  
  Serial.print("Config write received: ");
  Serial.print(len);
  Serial.println(" bytes");
  
  // Deserialize and apply config
  if (instance_->deserializeConfig(data, len)) {
    // Save to flash
    if (instance_->storage_ && instance_->config_) {
      if (instance_->storage_->saveConfig(instance_->config_)) {
        // Refresh the read characteristic so a subsequent BLE read returns the new values
        instance_->serializeConfig();
        instance_->sendStatus("Configuration saved", true);
        Serial.println("Configuration saved to flash");
      } else {
        instance_->sendStatus("Failed to save configuration", false);
        Serial.println("ERROR: Failed to save configuration");
      }
    }
  } else {
    instance_->sendStatus("Invalid configuration data", false);
    Serial.println("ERROR: Invalid configuration data");
  }
}

void BLEConfigService::calibrateCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
  if (!instance_ || !instance_->axes_ || !instance_->config_) return;
  
  if (len < 2) {
    Serial.println("ERROR: Invalid calibration command");
    return;
  }
  
  uint8_t axisIndex = data[0];
  uint8_t command = data[1];
  
  if (axisIndex >= MAX_AXES) {
    Serial.println("ERROR: Invalid axis index");
    return;
  }
  
  Serial.print("Calibration command: Axis ");
  Serial.print(axisIndex);
  Serial.print(", Command ");
  Serial.println(command);
  
  AxisConfig* axisCfg = &instance_->config_->axes[axisIndex];
  uint16_t currentRaw = instance_->axes_->getRawValue(axisIndex);
  
  switch (command) {
    case CAL_MIN:
      axisCfg->min = currentRaw;
      instance_->sendStatus("Min calibrated", true);
      Serial.print("Axis ");
      Serial.print(axisIndex);
      Serial.print(" min set to ");
      Serial.println(currentRaw);
      break;
      
    case CAL_CENTER:
      axisCfg->center = currentRaw;
      instance_->sendStatus("Center calibrated", true);
      Serial.print("Axis ");
      Serial.print(axisIndex);
      Serial.print(" center set to ");
      Serial.println(currentRaw);
      break;
      
    case CAL_MAX:
      axisCfg->max = currentRaw;
      instance_->sendStatus("Max calibrated", true);
      Serial.print("Axis ");
      Serial.print(axisIndex);
      Serial.print(" max set to ");
      Serial.println(currentRaw);
      break;
      
    case CAL_SAVE:
      if (instance_->storage_->saveConfig(instance_->config_)) {
        instance_->sendStatus("Calibration saved", true);
        Serial.println("Calibration saved to flash");
        // Update config read characteristic
        instance_->serializeConfig();
      } else {
        instance_->sendStatus("Failed to save calibration", false);
        Serial.println("ERROR: Failed to save calibration");
      }
      break;
      
    case CAL_RESET:
      axisCfg->min = 0;
      axisCfg->center = 2048;
      axisCfg->max = 4095;
      instance_->sendStatus("Calibration reset", true);
      Serial.print("Axis ");
      Serial.print(axisIndex);
      Serial.println(" calibration reset to defaults");
      break;
      
    default:
      instance_->sendStatus("Unknown calibration command", false);
      Serial.println("ERROR: Unknown calibration command");
      break;
  }
}
