# Web Configurator Testing Guide

## Current Status

✅ **Working:**
- WebBluetooth device discovery and connection
- Battery level monitoring
- UI for pin configuration, calibration, button mapping
- Export/import configuration files
- Configuration state management in browser

⏳ **In Progress:**
- BLE GATT configuration service (firmware side)
- Read/write device configuration over BLE
- Real-time axis/button state monitoring

## How to Test

### 1. Start Local Server

The web configurator requires HTTPS for WebBluetooth (security requirement).

**Option A: Python HTTPS Server (using localhost exception)**
```bash
cd configurator
python3 -m http.server 8000
```
Then open: http://localhost:8000

**Option B: Using mkcert for proper HTTPS**
```bash
# Install mkcert (one-time)
brew install mkcert
mkcert -install

# Generate local certificates (one-time)
cd configurator
mkcert localhost 127.0.0.1 ::1

# Serve with HTTPS (requires Node.js)
npx http-server -S -C localhost+2.pem -K localhost+2-key.pem -p 8443
```
Then open: https://localhost:8443

### 2. Open in Chrome/Edge

WebBluetooth is only supported in:
- Chrome 56+
- Edge 79+
- Opera 43+

**NOT supported:** Firefox, Safari (as of Feb 2026)

### 3. Connect to Device

1. Make sure XIAO nRF52840 is powered and running SeedJoy firmware in BLE mode
2. Click "Connect Device" button
3. Select "SeedJoy" from the Bluetooth device list
4. Should connect successfully

**If you get "blocklisted UUID" error:**
- This is fixed in the latest version of ble.js
- Make sure you're using the updated code (HID UUID moved to optionalServices)

### 4. What Works Now

✅ **Battery monitoring:** Should display current battery level

✅ **UI interaction:** All tabs and settings work locally

✅ **Export/import:** Can save/load configuration as JSON file

❌ **Read device config:** Shows "not available" message

❌ **Write device config:** Shows "not available" message

❌ **Live axis monitoring:** Requires config service

### 5. Test Configuration Locally

1. Modify pin assignments in the UI
2. Adjust axis calibration settings
3. Map buttons
4. Click "Export Configuration"
5. Save JSON file
6. Click "Import Configuration" 
7. Load the JSON file
8. Verify settings are restored

## Next Steps (Firmware Development)

To enable full configuration functionality, the firmware needs:

1. **Add Custom GATT Service:**
   ```cpp
   // In ble_hid.h
   BLEService configService_;
   BLECharacteristic configReadCharacteristic_;
   BLECharacteristic configWriteCharacteristic_;
   BLECharacteristic configStatusCharacteristic_;
   ```

2. **Define Service UUIDs:**
   ```cpp
   #define CONFIG_SERVICE_UUID "e95d0001-251d-470a-a062-fa1922dfa9a8"
   #define CONFIG_CHAR_UUID    "e95d0002-251d-470a-a062-fa1922dfa9a8"
   #define STATUS_CHAR_UUID    "e95d0003-251d-470a-a062-fa1922dfa9a8"
   ```

3. **Implement Read/Write Handlers:**
   - Handle config read requests (serialize DeviceConfig to JSON)
   - Handle config write requests (deserialize and validate JSON)
   - Update Flash storage when config changes

4. **Add to Advertising:**
   ```cpp
   Bluefruit.Advertising.addService(configService_);
   ```

## Troubleshooting

**"WebBluetooth not supported"**
- Use Chrome, Edge, or Opera
- Update browser to latest version

**"User cancelled the requestDevice() chooser"**
- User clicked cancel - expected behavior

**"Connection failed"**
- Make sure device is in BLE mode (not USB)
- Check device is powered on
- Try resetting the XIAO board

**"Config service not available"**
- Expected - firmware doesn't implement it yet
- Can still uselocal configurator features

**HTTPS required error**
- localhost exception should work
- Otherwise use mkcert for local HTTPS certificates

## Developer Console

Open browser DevTools (F12) and check Console tab for:
- Connection status messages
- Service discovery results
- Error details

Example successful connection:
```
Requesting Bluetooth device...
Device selected: SeedJoy-1234
Connecting to GATT server...
Connected to GATT server
Battery service connected, level: 100%
⚠️  Config service not available
    Configuration service not yet implemented in firmware
```

## Configuration File Format

The exported JSON configuration looks like:
```json
{
  "magic": 1592942775,
  "configVersion": 1,
  "deviceName": "SeedJoy",
  "mode": 1,
  "axes": [...],
  "buttons": [...],
  "shiftRegisters": {...},
  "usbVID": 9114,
  "usbPID": 32980,
  ...
}
```

You can manually edit this file and import it.
