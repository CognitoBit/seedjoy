# SeedJoy - BLE/USB Game Controller Firmware

A dual-mode (USB/BLE) game controller firmware for Seeed Studio XIAO nRF52840, inspired by FreeJoy.

## Features

- **4 analog axes** (12-bit resolution)
- **16 digital buttons** with debouncing
- **Dual-mode operation**: USB HID or BLE HID with manual switching
- **Advanced calibration**: Per-axis min/max, center, deadzone, and curves
- **Web-based configurator**: Zero-install configuration via WebBluetooth
- **Persistent configuration**: Stored in Flash memory
- **Low latency**: <1ms in USB mode, ~7.5-15ms in BLE mode
- **Battery support**: LiPo with USB charging

## Hardware Requirements

- **Board**: Seeed Studio XIAO nRF52840
- **Analog inputs**: 4x potentiometers (10kΩ recommended) on A0-A3
- **Digital inputs**: 16x buttons with pull-up resistors (or use internal pull-ups)
- **Optional**: LiPo battery (3.7V, 100-500mAh) for wireless operation
- **Optional**: Mode selection button on D0

## Pin Configuration (Default)

### Analog Axes
- A0 (P0.02) - Axis 1 (X)
- A1 (P0.03) - Axis 2 (Y)
- A2 (P0.28) - Axis 3 (Z)
- A3 (P0.29) - Axis 4 (Rz)

### Buttons
- D1 (P0.04) - Button 1
- D2 (P0.05) - Button 2
- D3 (P0.06) - Button 3
- D4 (P0.07) - Button 4
- D5 (P0.08) - Button 5
- D6 (P0.09) - Button 6
- D7 (P0.10) - Button 7
- D8 (P0.11) - Button 8
- D9 (P0.12) - Button 9
- D10 (P0.13) - Button 10
- MOSI (P0.26) - Button 11
- MISO (P0.27) - Button 12
- SCK (P0.30) - Button 13
- TX (P0.31) - Button 14
- RX (P0.00) - Button 15
- SCL (P0.01) - Button 16

### Special
- D0 (P1.11) - Mode selection (hold on boot: USB if HIGH, BLE if LOW)
- LED_RED - Status indicator
- LED_BLUE - Connection indicator

## Quick Start

### 1. Flash Firmware

#### Option A: Arduino CLI (Recommended)
```bash
cd firmware/seedjoy
~/bin/arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 --output-dir build .
~/bin/arduino-cli upload -p /dev/cu.usbmodem* --fqbn Seeeduino:nrf52:xiaonRF52840 --input-dir build .
```

Or double-click reset button and drag `build/seedjoy.ino.zip` to the XIAO drive.

#### Option B: Arduino IDE
1. Install Arduino IDE 2.0+
2. Add board manager URL in Preferences:
   - Seeeduino: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
3. Install "Seeed nRF52 Boards" from Board Manager
4. Open `firmware/seedjoy/seedjoy.ino`
5. **Important**: Select board **"Seeed XIAO nRF52840"** (NOT "Sense")
6. If compilation fails, clear cache and restart IDE:
   ```bash
   rm -rf ~/Library/Caches/arduino/*
   ```
7. Upload to board

#### Option B: Pre-built UF2
1. Download latest `seedjoy-firmware.uf2` from Releases
2. Double-click RESET button on XIAO to enter bootloader mode
3. Drag `seedjoy-firmware.uf2` to the XIAO drive that appears

### 2. Select Mode

- **USB Mode**: Hold mode button (D0) HIGH during boot, or keep USB connected
- **BLE Mode**: Hold mode button (D0) LOW during boot, or disconnect USB
- Mode is saved and persists across reboots

### 3. Connect to PC

#### USB Mode
- Plug in USB-C cable
- Device appears as "SeedJoy Controller" in system joystick settings
- Test in Windows Game Controllers or Linux `jstest`

#### BLE Mode
- Open Bluetooth settings
- Pair with "SeedJoy-XXXX" (XXXX = last 4 chars of MAC)
- Device appears as HID gamepad
- **Note**: BLE HID may require pairing on first use

### 4. Configure

Open the web configurator and connect to your device:

1. Open configurator: `file:///path/to/configurator/index.html` (or serve locally with HTTPS)
2. Click "Connect Device" to pair via WebBluetooth
3. The device automatically enters **Configuration Mode** for 10 seconds:
   - HID input is disabled to prevent unwanted keystrokes/mouse movements
   - Hold the MODE button to stay in configuration mode
   - Send `H` via serial to enable HID immediately
4. **Read Configuration**: Click "Read from Device" to download current settings
5. **Modify Settings**: 
   - Calibrate axes with live preview
   - Remap buttons
   - Adjust deadzones and curves
6. **Write Configuration**: Click "Write to Device" to save settings to Flash
7. Configuration is automatically saved and persists across reboots

**Features:**
- ✅ Read/write configuration via BLE
- ✅ Real-time axis monitoring with live graphs
- ✅ Remote calibration (min/center/max)
- ✅ Battery level monitoring
- ✅ Automatic configuration mode (prevents HID interference)
- ✅ Export/import configuration profiles (localStorage)

## Configuration

### Web Configurator

The web-based configurator uses WebBluetooth API (requires Chrome/Edge/Opera).

**Features:**
- Pin assignment (visual pinout diagram)
- Axis calibration wizard with live preview
- Deadzone and curve configuration
- Button mapping and testing
- Firmware update (UF2 upload)
- Export/import configuration profiles

**Offline Use:**
Download the configurator from Releases and open `index.html` in your browser.

### Manual Configuration

Configuration is stored as JSON in Flash at address `0x7C000`. You can also configure via serial commands (see `docs/protocol.md`).

## Development

### Build Firmware

```bash
cd firmware/seedjoy

# Arduino CLI (recommended)
~/bin/arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 --output-dir build .

# Build outputs in build/ directory:
# - seedjoy.ino.hex (for programmer)
# - seedjoy.ino.zip (for UF2 bootloader)
# - seedjoy.ino.elf (debug symbols)
```

**Upload:**
```bash
# Method 1: Arduino CLI
~/bin/arduino-cli upload -p /dev/cu.usbmodem* --fqbn Seeeduino:nrf52:xiaonRF52840 --input-dir build .

# Method 2: UF2 Bootloader (drag & drop)
# 1. Double-click reset button on XIAO
# 2. Drag build/seedjoy.ino.zip to XIAO drive
```

### Test Configurator Locally

```bash
cd configurator
# Serve with Python
python3 -m http.server 8000

# Or Node.js
npx http-server -p 8000

# Open https://localhost:8000 (requires HTTPS for WebBluetooth)
# For HTTPS, use ngrok or mkcert
```

### Debug

- **Serial Monitor**: 115200 baud, prints boot info, mode, config changes
- **BLE Sniffer**: Use nRF Sniffer for Bluetooth LE to debug BLE packets
- **USB HID**: Use Wireshark with USBPcap to capture HID reports

## Architecture

```
firmware/
├── seedjoy/
│   ├── seedjoy.ino       # Main sketch (setup, loop, mode manager)
│   ├── config.h          # DeviceConfig structure
│   ├── axes.h/cpp        # Analog axis processing
│   ├── buttons.h/cpp     # Button debouncing
│   ├── usb_hid.h/cpp     # USB HID implementation
│   ├── ble_hid.h/cpp     # BLE HID implementation
│   ├── storage.h/cpp     # Flash config storage
│   └── protocol.h/cpp    # Configuration protocol

configurator/
├── index.html            # Main UI
├── app.js                # Application logic
├── ble.js                # WebBluetooth communication
├── calibration.js        # Axis calibration wizard
├── styles.css            # Styling
└── assets/               # SVG pinout, icons

docs/
├── hardware.md           # Wiring diagrams
├── protocol.md           # Config protocol spec
└── api.md                # Web configurator API
```

## Roadmap

### MVP (Current)
- [x] 4 axes, 16 buttons
- [x] 74HC165 shift register support (up to 144 total buttons)
- [x] USB/BLE dual mode
- [x] Basic web configurator UI
- [x] WebBluetooth connection
- [x] Battery monitoring via BLE
- [x] BLE configuration service (custom GATT)
- [x] Axis calibration over BLE
- [x] Flash storage read/write over BLE
- [x] Configuration mode to prevent HID interference

### Phase 2
- [x] Real-time button monitoring in configurator
- [ ] Improved JSON parsing with ArduinoJson library
- [ ] Configuration import/export from configurator UI
- [ ] Firmware update via WebBluetooth (DFU)

### Future Enhancements
- [ ] 8 axes (via external ADC: ADS1115)
- [ ] Rotary encoders (2-4)
- [ ] LED support (status, backlighting)
- [ ] Shift layers (2x button count)
- [ ] Sensor support (TLE5011, AS5600)
- [ ] Button matrices (64+ buttons)
- [ ] Advanced power management
- [ ] OTA firmware updates via BLE DFU

## Troubleshooting

**Device not recognized in USB mode**
- Check USB cable (must support data, not charge-only)
- Try different USB port
- Reinstall drivers (Windows: device manager > update driver)

**Cannot pair in BLE mode**
- Clear Bluetooth pairing list
- Reset device (double-tap RESET button)
- Some OS require manual HID driver installation for BLE gamepads

**Axes drifting or jittering**
- Increase deadzone in configurator
- Use higher quality potentiometers
- Add capacitors (0.1µF) across potentiometer outputs
- Calibrate axes in configurator

**Low battery life in BLE mode**
- Reduce connection interval (latency trade-off)
- Enable sleep mode in advanced settings
- Use larger battery (500mAh recommended)

**Configuration not saving**
- Check that device is connected via BLE
- Verify config service is available (check serial output)
- Ensure latest firmware is uploaded
- Try "Read from Device" first to verify connection

## License

MIT License - see LICENSE file

## Credits

Inspired by [FreeJoy](https://github.com/FreeJoy-Team/FreeJoy) by Alexandr Yaroshenko

## Contributing

Contributions welcome! Please open issues for bugs or feature requests.

## Support

- **Documentation**: See `docs/` folder
- **Issues**: GitHub Issues
- **Community**: Discord (link TBD)
