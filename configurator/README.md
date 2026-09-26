# SeedJoy Web Configurator

A web-based configuration tool for the SeedJoy game controller using WebBluetooth API.

## Features

- **Zero-install:** Runs entirely in the browser
- **WebBluetooth:** Direct BLE communication with device
- **Visual pin configuration:** Interactive pinout diagram
- **Live axis calibration:** Real-time axis preview and calibration wizard
- **Button mapping:** Remap physical buttons to logical numbers
- **Advanced settings:** USB/BLE parameters, power management
- **Import/Export:** Save and load configuration profiles

## Requirements

### Browser
- Chrome 56+ (recommended)
- Edge 79+
- Opera 43+
- **Note:** WebBluetooth requires HTTPS or localhost

### Operating System
- **Windows 10+** (WebBluetooth support requires Windows 10 version 1706+)
- **macOS 10.12+**
- **Linux** (with BlueZ 5.41+)
- **Android 6.0+** (Chrome for Android)

**Not supported:** iOS (Safari doesn't support WebBluetooth)

## Running Locally

### Option 1: Python HTTP Server (Easiest)

```bash
cd configurator
python3 -m http.server 8000
```

Open browser to: `http://localhost:8000`

### Option 2: Node.js HTTP Server

```bash
cd configurator
npx http-server -p 8000
```

Open browser to: `http://localhost:8000`

### Option 3: VS Code Live Server

1. Install "Live Server" extension in VS Code
2. Right-click `index.html`
3. Select "Open with Live Server"

## Deploying to GitHub Pages

1. Fork/clone the repository
2. Enable GitHub Pages in repository settings
3. Set source to `main` branch, `/configurator` folder (or root if configurator is at root)
4. Access at: `https://yourusername.github.io/seedjoy/`

**Important:** GitHub Pages uses HTTPS by default, which is required for WebBluetooth!

## Usage

### 1. Connect to Device

1. Ensure your SeedJoy is powered on and in BLE mode
2. Click **Connect Device** button
3. Select **SeedJoy** (the configured device name) from the device list
4. Allow pairing if prompted

### 2. Read Configuration

1. Once connected, click **Read from Device**
2. Current configuration will be loaded from the device
3. UI will update to show current settings

### 3. Configure

Navigate through tabs to configure:

**Pin Configuration:**
- Assign axes to analog pins (A0-A3)
- Assign buttons to digital pins
- Validate pin assignments

**Axis Calibration:**
- Select axis to calibrate
- Move axis to min/center/max positions
- Click calibration buttons
- Adjust deadzone, curves, smoothing

**Button Mapping:**
- Remap physical buttons to logical numbers
- Invert button logic if needed
- Test buttons in real-time

**Advanced Settings:**
- Change device name
- Set operation mode (USB/BLE/Auto)
- Adjust USB poll rate
- Configure BLE connection interval and TX power
- Set up power management

### 4. Write to Device

1. After making changes, click **Write to Device**
2. Confirm the operation
3. Configuration will be saved to device Flash memory
4. Device will automatically apply new settings

### 5. Export/Import Configurations

**Export:**
- Click **Export Config**
- JSON file will download with timestamp
- Save for backup or sharing

**Import:**
- Click **Import Config**
- Select previously exported JSON file
- Configuration will load into UI
- Click **Write to Device** to apply

## Troubleshooting

### "Bluetooth not available"
- Check if WebBluetooth is enabled in browser flags (chrome://flags)
- Ensure Bluetooth is turned on in OS settings
- Try Chrome/Edge/Opera (WebBluetooth support required)

### "No device found"
- Ensure SeedJoy is powered on and in BLE mode
- Check the mode select: D9 must be connected to GND at boot for BLE mode
- Move closer to device (within 10m)
- Reset device and try again

### "Failed to connect"
- Unpair device in OS Bluetooth settings
- Restart browser
- Reset SeedJoy device
- Clear browser cache

### "Can't read/write configuration"
- Ensure device is connected (green indicator)
- Check firmware supports config service
- Try reconnecting
- Check serial monitor for errors

### "Configuration invalid"
- Imported config must match current firmware version
- Check JSON structure is correct
- Try resetting to defaults first
- Verify pin assignments don't conflict

### "Changes not saving"
- Must click "Write to Device" to save to Flash
- Check device connection is stable
- Verify no errors in browser console (F12)
- Try reading config again to verify

### HTTPS Required Error

WebBluetooth requires HTTPS or localhost. If you see this error:

**For local development:**
- Use `localhost` instead of `127.0.0.1`
- Or use Python/Node server (see "Running Locally")

**For production:**
- Deploy to GitHub Pages (free HTTPS)
- Use Netlify/Vercel (free HTTPS)
- Set up SSL certificate on your server

## File Structure

```
configurator/
├── index.html          # Main UI
├── styles.css          # Styling
├── app.js              # Main application logic
├── ble.js              # WebBluetooth communication
├── config.js           # Configuration management
├── calibration.js      # Axis calibration
└── assets/             # Images, icons (optional)
```

## Development

### Modifying the UI

Edit `index.html` and `styles.css` to customize appearance.

### Adding Features

1. **New configuration field:**
   - Add to `SeedJoyConfig.getDefaultConfig()` in `config.js`
   - Add UI element in `index.html`
   - Add event handler in `app.js`

2. **New calibration feature:**
   - Extend `CalibrationManager` in `calibration.js`
   - Add UI controls in axis calibration tab

3. **New BLE characteristic:**
   - Add UUID in `ble.js`
   - Implement read/write methods
   - Update protocol documentation

### Testing

Test with mock device (when not connected):
- Axis preview uses simulated values
- Button test can be triggered via keyboard (future enhancement)
- Configuration changes work offline, just can't sync to device

### Browser Console

Open Developer Tools (F12) to see:
- Connection status logs
- Configuration read/write operations
- Error messages
- BLE characteristic data

## API Reference

### SeedJoyBLE Class

```javascript
const ble = new SeedJoyBLE();

// Connect to device
await ble.connect();

// Check connection
if (ble.isConnected()) { ... }

// Read config
const config = await ble.readConfig();

// Write config
await ble.writeConfig(config);

// Read status
const status = await ble.readStatus();

// Disconnect
await ble.disconnect();

// Callbacks
ble.onConnectionChange = (connected) => { ... };
ble.onBatteryChange = (level) => { ... };
```

### SeedJoyConfig Class

```javascript
const config = new SeedJoyConfig();

// Get default config
const defaultConfig = config.getDefaultConfig();

// Update axis
config.updateAxis(0, { deadzone: 10 });

// Update button
config.updateButton(0, { logicalNumber: 5 });

// Update device settings
config.updateDeviceSettings({ deviceName: "MyStick" });

// Export to file
config.exportToFile();

// Import from file
await config.importFromFile(file);

// Reset to defaults
config.resetToDefaults();
```

### CalibrationManager Class

```javascript
const calibration = new CalibrationManager(ble, config);

// Start live monitoring
calibration.startMonitoring();

// Set axis for calibration
calibration.setCurrentAxis(0);

// Calibrate points
await calibration.calibrateMin();
await calibration.calibrateCenter();
await calibration.calibrateMax();

// Stop monitoring
calibration.stopMonitoring();
```

## Contributing

Contributions welcome! Areas for improvement:
- Better mobile support
- Dark mode toggle
- SVG pinout diagram
- Custom curve editor (canvas-based)
- Button macro system
- Telemetry/analytics view
- OTA firmware update UI

## License

MIT License - see root LICENSE file

## See Also

- [Main README](../README.md)
- [Getting Started Guide](../docs/getting-started.md)
- [Configuration Protocol](../docs/protocol.md)
- [Hardware Wiring](../docs/hardware.md)
