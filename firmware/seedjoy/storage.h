/**
 * SeedJoy - Flash Storage Manager
 * 
 * Handles reading/writing configuration to Flash memory
 */

#ifndef STORAGE_H
#define STORAGE_H

#include "config.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

using namespace Adafruit_LittleFS_Namespace;

class StorageManager {
public:
  StorageManager();
  
  // Initialize storage (call in setup)
  bool begin();
  
  // Load configuration from Flash
  bool loadConfig(DeviceConfig* config);
  
  // Save configuration to Flash
  bool saveConfig(const DeviceConfig* config);
  
  // Reset to factory defaults
  bool resetToDefaults();
  
  // Check if valid config exists
  bool hasValidConfig();
  
private:
  static const char* CONFIG_FILENAME;
  
  bool writeConfigFile(const DeviceConfig* config);
  bool readConfigFile(DeviceConfig* config);
};

#endif // STORAGE_H
