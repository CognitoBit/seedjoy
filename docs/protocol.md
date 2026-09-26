# Configuration Protocol

This document describes the configuration protocol used to communicate between the web configurator and the SeedJoy firmware.

## Overview

SeedJoy supports configuration via:
1. **BLE GATT Characteristics** (primary method for wireless config)
2. **Serial Commands** (development/debugging)
3. **USB HID Reports** (future enhancement)

## BLE Configuration Service

### Service UUID
```
CONFIG_SERVICE_UUID = e95d0001-251d-470a-a062-fa1922dfa9a8
```

### Characteristics

#### 1. Config Characteristic (Read/Write)
```
UUID: e95d0002-251d-470a-a062-fa1922dfa9a8
Properties: Read, Write
Max Size: 512 bytes (may require fragmentation)
```

**Purpose:** Transfer full device configuration

**Format:** JSON (UTF-8 encoded)

**Example Read Response:**
```json
{
  "magic": 1593968567,
  "configVersion": 1,
  "firmwareMajor": 0,
  "firmwareMinor": 1,
  "firmwarePatch": 0,
  "deviceName": "SeedJoy",
  "mode": 0,
  "usbVID": 9242,
  "usbPID": 32500,
  "usbPollRate": 1,
  "bleConnInterval": 2,
  "bleTxPower": 0,
  "axes": [
    {
      "pin": 2,
      "enabled": true,
      "inverted": false,
      "min": 0,
      "center": 2048,
      "max": 4095,
      "deadzone": 5,
      "curveType": 0,
      "expoFactor": 0.0,
      "customCurve": [0.0, 0.25, 0.5, 0.75, 1.0],
      "smoothing": 0
    },
    // ... 3 more axes
  ],
  "buttons": [
    {
      "pin": 4,
      "enabled": true,
      "logicalNumber": 0,
      "inverted": false
    },
    // ... 15 more buttons
  ],
  "autoSleep": false,
  "sleepTimeout": 600,
  "crc32": 305441816
}
```

**Write:** Send JSON configuration to update device settings. Device will validate, calculate CRC, and store in Flash.

#### 2. Status Characteristic (Read, Notify)
```
UUID: e95d0004-251d-470a-a062-fa1922dfa9a8
Properties: Read, Notify
Max Size: 256 bytes
```

**Purpose:** Query device status and real-time axis/button values

**Format:** JSON (UTF-8 encoded)

**Example Response:**
```json
{
  "mode": "USB",
  "firmwareVersion": "0.1.0",
  "connected": true,
  "batteryLevel": 87,
  "axes": [
    { "raw": 2048, "processed": 0 },
    { "raw": 1024, "processed": -16384 },
    { "raw": 3072, "processed": 16384 },
    { "raw": 2048, "processed": 0 }
  ],
  "buttons": 0b0000000000001101
}
```

**Notifications:** Subscribe to get real-time updates (useful for calibration/testing)

## Serial Commands

For debugging and development, configuration can be done via USB Serial.

### Format
```
COMMAND [ARGS]\n
```

### Commands

#### GET_CONFIG
Get full configuration as JSON
```
GET_CONFIG
```

Response:
```json
{...}
```

#### SET_CONFIG
Set configuration from JSON (paste entire JSON on one line)
```
SET_CONFIG {"magic": 1593968567, "configVersion": 1, ...}
```

Response:
```
OK
```

#### GET_STATUS
Get current status
```
GET_STATUS
```

Response:
```json
{"mode": "USB", "firmwareVersion": "0.1.0", ...}
```

#### CALIBRATE_AXIS
Calibrate axis min/center/max
```
CALIBRATE_AXIS <axis_index> <min|center|max>
```

Example:
```
CALIBRATE_AXIS 0 min    # Set axis 0 minimum to current value
CALIBRATE_AXIS 0 center # Set axis 0 center to current value
CALIBRATE_AXIS 0 max    # Set axis 0 maximum to current value
```

#### RESET_CONFIG
Reset to default configuration
```
RESET_CONFIG
```

#### REBOOT
Reboot device
```
REBOOT
```

#### SET_MODE
Change operation mode
```
SET_MODE <usb|ble|auto>
```

#### GET_AXIS
Get current axis raw value
```
GET_AXIS <index>
```

Response:
```
AXIS 0: raw=2048 processed=0
```

#### GET_BUTTONS
Get current button states
```
GET_BUTTONS
```

Response:
```
BUTTONS: 0b0000000000001101 (0x000D)
  0: 1
  1: 0
  2: 1
  3: 1
  ...
```

## Configuration Structure

### DeviceConfig

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| magic | uint32 | 0x5EED70B7 | Magic number for validation |
| configVersion | uint8 | 1 | Config structure version |
| firmwareMajor | uint8 | 0 | Firmware major version |
| firmwareMinor | uint8 | 1 | Firmware minor version |
| firmwarePatch | uint8 | 0 | Firmware patch version |
| deviceName | char[32] | "SeedJoy" | BLE device name |
| mode | uint8 | 0 | 0=USB, 1=BLE, 2=AUTO |
| usbVID | uint16 | 0x239A | USB Vendor ID |
| usbPID | uint16 | 0x80F4 | USB Product ID |
| usbPollRate | uint8 | 1 | USB poll rate (ms) |
| bleConnInterval | uint16 | 2 | BLE connection interval (1.25ms units) |
| bleTxPower | int8 | 0 | BLE TX power (dBm) |
| axes | AxisConfig[4] | See below | Axis configurations |
| buttons | ButtonConfig[16] | See below | Button configurations |
| autoSleep | bool | false | Auto sleep enabled |
| sleepTimeout | uint16 | 600 | Sleep timeout (seconds) |
| crc32 | uint32 | calculated | CRC32 checksum |

### AxisConfig

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| pin | uint8 | 0–3 | Arduino analog pin (0 = A0 … 3 = A3), see Pin Numbers |
| enabled | bool | false | Axis enabled (off by default; floating ADC pins cause noise) |
| inverted | bool | false | Invert axis direction |
| min | uint16 | 0 | Calibrated minimum (0-4095) |
| center | uint16 | 2048 | Calibrated center (0-4095) |
| max | uint16 | 4095 | Calibrated maximum (0-4095) |
| deadzone | uint8 | 5 | Deadzone percentage (0-50) |
| curveType | uint8 | 0 | 0=Linear, 1=Expo, 2=Custom |
| expoFactor | float | 0.0 | Exponential factor (0.0-1.0) |
| customCurve | float[5] | [0.0, 0.25, 0.5, 0.75, 1.0] | Custom curve points |
| smoothing | uint8 | 0 | Smoothing samples (0, 2, 4, 8) |

### ButtonConfig

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| pin | uint8 | 4, 5, 10, 255… | Arduino pin number, see Pin Numbers |
| enabled | bool | false | Button enabled (off by default; SR-only mode) |
| logicalNumber | uint8 | index | Logical button number (0-15) |
| inverted | bool | false | Invert logic (HIGH=pressed) |

## Pin Numbers

All `pin`, `dataPin`, `clockPin` and `loadPin` values are **Arduino pin numbers** for the Seeed
XIAO nRF52840 core, i.e. the numbers the firmware passes straight to `pinMode()` /
`digitalRead()` / `analogRead()`. They are **not** nRF52840 GPIO (P0.xx / P1.xx) numbers.
`255` (`0xFF`) means "not used".

| Value | XIAO pin | nRF52840 port | Default use |
|-------|----------|---------------|-------------|
| 0  | A0 / D0 | P0.02 | Axis 0 |
| 1  | A1 / D1 | P0.03 | Axis 1 |
| 2  | A2 / D2 | P0.28 | Axis 2 |
| 3  | A3 / D3 | P0.29 | Axis 3 |
| 4  | D4 (SDA) | P0.04 | GPIO button (disabled) |
| 5  | D5 (SCL) | P0.05 | GPIO button (disabled) |
| 6  | D6 (TX) | P1.11 | 74HC165 data (QH) |
| 7  | D7 (RX) | P1.12 | 74HC165 clock |
| 8  | D8 (SCK) | P1.13 | 74HC165 load (SH/LD) |
| 9  | D9 (MISO) | P1.14 | Mode select, don't assign |
| 10 | D10 (MOSI) | P1.15 | GPIO button (disabled) |

Only values 0–3 are valid for axes (ADC-capable pins wired to the XIAO header).

## CRC32 Calculation

The firmware calculates CRC32 over the entire config structure excluding the `crc32` field itself.

**Algorithm:** Standard CRC32 (polynomial 0xEDB88320)

**JavaScript Implementation:**
```javascript
function calculateCRC32(config) {
    // Exclude crc32 field
    const data = JSON.stringify({...config, crc32: 0});
    const bytes = new TextEncoder().encode(data);
    
    let crc = 0xFFFFFFFF;
    for (let i = 0; i < bytes.length; i++) {
        crc ^= bytes[i];
        for (let j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >>> 1) ^ 0xEDB88320;
            } else {
                crc >>>= 1;
            }
        }
    }
    return ~crc >>> 0;
}
```

## Error Handling

### Validation Errors

If config validation fails, device returns error:

**BLE:** Write fails with GATT error
**Serial:** 
```
ERROR: Invalid magic number
ERROR: Invalid config version
ERROR: CRC mismatch
ERROR: Invalid pin assignment
```

### Recovery

If config is corrupted:
1. Device boots with default configuration
2. Serial monitor shows warning: "Config validation failed"
3. Use web configurator or serial to write new config
4. Or send `RESET_CONFIG` command

## Example Workflows

### Calibrate Axis via Serial

```
# Connect to serial monitor (115200 baud)

# Check current axis 0 value
GET_AXIS 0
> AXIS 0: raw=150 processed=-32767

# Move axis to minimum position
CALIBRATE_AXIS 0 min
> OK: Axis 0 min set to 150

# Move to center
CALIBRATE_AXIS 0 center
> OK: Axis 0 center set to 2048

# Move to maximum
CALIBRATE_AXIS 0 max
> OK: Axis 0 max set to 3950

# Verify
GET_CONFIG
> {...}
```

### Update via Web Configurator

```javascript
// In configurator JavaScript

// Read config
const config = await ble.readConfig();

// Modify
config.axes[0].deadzone = 10;
config.deviceName = "MyFlightStick";

// Write back
await ble.writeConfig(config);
```

## Future Enhancements

- **Firmware Update Protocol:** OTA updates via BLE DFU
- **Profile System:** Save/load multiple configurations
- **Macro Support:** Button combinations trigger sequences
- **Telemetry:** Axis/button usage statistics
- **USB HID Config:** Configure via USB without BLE

## See Also

- [Getting Started Guide](getting-started.md)
- [Hardware Wiring](hardware.md)
- [Web Configurator Documentation](../configurator/README.md)
