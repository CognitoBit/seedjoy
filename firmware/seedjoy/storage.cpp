/**
 * SeedJoy - Flash Storage Manager Implementation
 */

#include "storage.h"

const char* StorageManager::CONFIG_FILENAME = "/seedjoy_config.dat";

StorageManager::StorageManager() {
}

bool StorageManager::begin() {
  // Initialize the internal file system
  InternalFS.begin();
  return true;
}

// Older configurator builds wrote pseudo nRF "P-number" pin values (e.g. D6 as
// 0x2B = 43) that are not valid Arduino pins on the XIAO (D0..D10 = 0..10).
// Repair any such value so a board flashed with this firmware works again
// without the user having to rewrite its config.
static void sanitizePins(DeviceConfig* config) {
  const uint8_t MAX_XIAO_PIN = 10;  // D10
  bool fixed = false;
  ShiftRegisterConfig& sr = config->shiftRegisters;
  if ((sr.dataPin  > MAX_XIAO_PIN && sr.dataPin  != 0xFF) ||
      (sr.clockPin > MAX_XIAO_PIN && sr.clockPin != 0xFF) ||
      (sr.loadPin  > MAX_XIAO_PIN && sr.loadPin  != 0xFF)) {
    sr.dataPin = SR_DATA_PIN; sr.clockPin = SR_CLOCK_PIN; sr.loadPin = SR_LOAD_PIN;
    fixed = true;
  }
  const uint8_t axisDefaults[MAX_AXES] = { AXIS_0_PIN, AXIS_1_PIN, AXIS_2_PIN, AXIS_3_PIN };
  for (int i = 0; i < MAX_AXES; i++) {
    if (config->axes[i].pin > A3 && config->axes[i].pin != 0xFF) {
      config->axes[i].pin = axisDefaults[i];
      fixed = true;
    }
  }
  for (int i = 0; i < MAX_BUTTONS; i++) {
    if (config->buttons[i].pin > MAX_XIAO_PIN && config->buttons[i].pin != 0xFF) {
      config->buttons[i].pin = 0xFF;
      config->buttons[i].enabled = false;
      fixed = true;
    }
  }
  if (fixed) Serial.println("Repaired invalid pin numbers in stored config");
}

bool StorageManager::loadConfig(DeviceConfig* config) {
  if (!config) return false;
  
  // Try to read from file
  if (readConfigFile(config)) {
    // Validate the loaded config
    if (validateConfig(config)) {
      Serial.println("Config loaded from Flash");
      sanitizePins(config);
      return true;
    } else {
      Serial.println("Config validation failed");
    }
  }
  
  // If load failed, use defaults
  Serial.println("Using default configuration");
  *config = DeviceConfig();  // Reset to defaults
  return false;
}

bool StorageManager::saveConfig(const DeviceConfig* config) {
  if (!config) return false;
  
  // Calculate CRC before saving
  DeviceConfig tempConfig = *config;
  tempConfig.crc32 = calculateCRC32(&tempConfig);
  
  if (writeConfigFile(&tempConfig)) {
    Serial.println("Config saved to Flash");
    return true;
  }
  
  Serial.println("Failed to save config");
  return false;
}

bool StorageManager::resetToDefaults() {
  DeviceConfig defaultConfig;
  return saveConfig(&defaultConfig);
}

bool StorageManager::hasValidConfig() {
  DeviceConfig testConfig;
  return readConfigFile(&testConfig) && validateConfig(&testConfig);
}

bool StorageManager::writeConfigFile(const DeviceConfig* config) {
  // Remove any existing file first. Adafruit LittleFS opens FILE_O_WRITE in
  // append mode (seek-to-end, no truncate), so without this every save would
  // append another full struct and grow the file, failing the size check on
  // next boot and silently reverting to defaults.
  InternalFS.remove(CONFIG_FILENAME);

  // Open file for writing
  File configFile = InternalFS.open(CONFIG_FILENAME, FILE_O_WRITE);
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  
  // Write the entire structure
  size_t written = configFile.write((uint8_t*)config, sizeof(DeviceConfig));
  configFile.close();
  
  if (written != sizeof(DeviceConfig)) {
    Serial.print("Write size mismatch: ");
    Serial.print(written);
    Serial.print(" != ");
    Serial.println(sizeof(DeviceConfig));
    return false;
  }
  
  return true;
}

bool StorageManager::readConfigFile(DeviceConfig* config) {
  // Check if file exists
  if (!InternalFS.exists(CONFIG_FILENAME)) {
    Serial.println("Config file does not exist");
    return false;
  }
  
  // Open file for reading
  File configFile = InternalFS.open(CONFIG_FILENAME, FILE_O_READ);
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    return false;
  }
  
  // Check file size
  uint32_t fileSize = configFile.size();
  if (fileSize != sizeof(DeviceConfig)) {
    Serial.print("Config file size mismatch: ");
    Serial.print(fileSize);
    Serial.print(" != ");
    Serial.println(sizeof(DeviceConfig));
    configFile.close();
    return false;
  }
  
  // Read the entire structure
  size_t bytesRead = configFile.read((uint8_t*)config, sizeof(DeviceConfig));
  configFile.close();
  
  if (bytesRead != sizeof(DeviceConfig)) {
    Serial.println("Failed to read complete config");
    return false;
  }
  
  return true;
}

// CRC32 calculation (from config.h)
uint32_t calculateCRC32(const DeviceConfig* config) {
  // Simple CRC32 implementation
  const uint8_t* data = (const uint8_t*)config;
  size_t length = sizeof(DeviceConfig) - sizeof(uint32_t); // Exclude CRC field
  
  uint32_t crc = 0xFFFFFFFF;
  
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  
  return ~crc;
}

// Validate config structure (from config.h)
bool validateConfig(const DeviceConfig* config) {
  if (!config) return false;
  
  // Check magic number
  if (config->magic != 0x5EED70B7) {
    Serial.println("Invalid magic number");
    return false;
  }
  
  // Check config version
  if (config->configVersion != 1) {
    Serial.println("Unsupported config version");
    return false;
  }
  
  // Verify CRC
  uint32_t calculatedCRC = calculateCRC32(config);
  if (config->crc32 != calculatedCRC) {
    Serial.print("CRC mismatch: ");
    Serial.print(config->crc32, HEX);
    Serial.print(" != ");
    Serial.println(calculatedCRC, HEX);
    return false;
  }
  
  return true;
}
