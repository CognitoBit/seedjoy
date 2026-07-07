# SeedJoy

A dual-mode (USB HID / BLE HID) game controller firmware for the **Seeed Studio XIAO nRF52840**. Supports up to **56 buttons via 74HC165 shift registers** and **4 analog axes**, configurable through a zero-install web app over either **WebBluetooth** or **Web Serial**.

---

## Features

- **56 buttons** via 7× 74HC165 shift registers (SR-only mode, no GPIO buttons needed)
- **Up to 64 total buttons** when mixing SR + GPIO inputs
- **4 analog axes** (12-bit, A0–A3)
- **Dual transport**: USB HID or BLE HID, selectable at boot
- **Web configurator**: Chrome/Edge — no install, no server
  - Connect via **WebBluetooth** (wireless) or **Web Serial** (USB cable)
  - Read / write full device config (SR settings, axes, buttons, power)
  - Real-time axis calibration with live preview
  - Real-time **button test tab** — 56 indicators light up as you press buttons (both BLE and serial)
- **Persistent config**: LittleFS flash, CRC32-validated
- **Ghost-input protection**: 60-second config window on BLE connect during which HID is disabled
- **Battery monitoring**: LiPo voltage → BLE battery service

---

## Hardware

### Required

| Part | Qty | Notes |
|------|-----|-------|
| Seeed XIAO nRF52840 | 1 | **Non-Sense** variant |
| 74HC165 8-bit PISO shift register | 7 | Daisy-chained for 56 buttons |
| 10 kΩ resistors | 56 | Pull-ups on each button input |
| Tactile buttons / switches | up to 56 | |

### Optional

| Part | Notes |
|------|-------|
| 10 kΩ potentiometers (×4) | Analog axes on A0–A3 |
| LiPo battery (3.7 V, 100–500 mAh) | Wireless operation |

### Shift Register Wiring (74HC165)

Default pins — configurable in software:

| XIAO Pin | SR Pin | Function |
|----------|--------|----------|
| D6 (P1.11) | QH (serial out, last chip) | MISO / data |
| D7 (P1.12) | CLK | Clock |
| D8 (P1.13) | SH/LD̄ | Load / latch |
| 3.3 V | VCC | Power |
| GND | GND | Ground |

Daisy-chain: QH of chip N → SER (pin 10) of chip N+1. First chip's SER tied to GND (or VCC if `inverted: true`).

Use a 10 kΩ pull-up resistor on each button input pin (Dn) of each 74HC165.

**Button numbering** (SR-only mode):
- Chip 1, bit 0 → Button 0 (HID button 1)
- Chip 1, bit 7 → Button 7
- Chip 2, bit 0 → Button 8
- …
- Chip 7, bit 7 → Button 55

### Special / Reserved Pins

| Pin | Function |
|-----|----------|
| D9 (P1.14) | Mode select (HIGH = USB, LOW = BLE) |
| LED\_RED | Status (blinks on boot) |
| LED\_BLUE | BLE connection indicator |
| VBAT | Battery voltage sense |

---

## Quick Start

### 1. Install Board Support

In Arduino IDE (2.0+) or Arduino CLI, add the Seeed board manager URL:
```
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
```
Install: **Seeed nRF52 Boards** (NOT the mbed-based package).

Select board: **Seeed XIAO nRF52840** (not Sense).

### 2. Flash Firmware

#### Arduino CLI
```bash
cd firmware/seedjoy
chmod +x compile.sh upload.sh
./compile.sh          # builds into build/
./upload.sh           # uploads via USB
```

#### Arduino IDE
Open `firmware/seedjoy/seedjoy.ino` → Upload.

#### UF2 drag-and-drop
1. Double-click the RESET button to enter bootloader (drive named `XIAONRF52` appears)
2. Drag `build/seedjoy.ino.zip` onto the drive

### 3. Wire Shift Registers

Connect D6/D7/D8 as described above and daisy-chain up to 7× 74HC165. The default config has SR enabled with 7 chips, so it works immediately without any configuration step.

### 4. Open Configurator

Open `configurator/index.html` directly in Chrome or Edge (no server needed for Web Serial; WebBluetooth requires HTTPS — use `npx http-server` or `python3 -m http.server` + ngrok/mkcert if needed).

Click **Connect** and choose **Serial** (USB cable) or **Bluetooth** (wireless, BLE mode only).

---

## Web Configurator

### Connection Modes

| Mode | Transport | Firmware mode |
|------|-----------|--------------|
| Web Serial | USB cable (115200 baud) | USB or BLE |
| WebBluetooth | Wireless GATT | BLE only |

### Workflow

1. **Connect** (Serial or Bluetooth)
2. **Read Config** — downloads all settings from the device into the UI
3. Edit shift register, axis, and button settings
4. **Write Config** — saves to device flash; SR hardware re-initialises live (no reboot)
5. Switch to **Button Mapping** tab — **56 button indicators** update in real time as you press your buttons
   - Over serial: firmware streams button states at 20 Hz automatically
   - Over BLE: firmware pushes button state updates via GATT notifications

### Configuration Sections

| Section | Fields |
|---------|--------|
| Device | Name, USB/BLE mode |
| USB/BLE | Poll rate, connection interval, TX power |
| Power | Auto-sleep, sleep timeout |
| Shift Registers | Enable, chip count (1–7), data/clock/load pins, invert |
| Axes (0–3) | Enable, pin, min/center/max cal, deadzone, curve, invert, smoothing |
| GPIO Buttons (0–15) | Enable, pin, logical number, invert |

### Safety — Ghost Input Protection

On BLE connect, the firmware enters **Configuration Mode** for **60 seconds**:
- All HID reports are suppressed
- The BLUE LED is on
- Serial prints `BLE CONNECTED - Configuration Mode Active`

This prevents floating/unconfigured pins from generating random HID input while you configure the device. HID activates after the 60-second window, or immediately when you send `H` via serial. Hold the D9 MODE button to keep config mode active indefinitely.

> ⚠️ Enable only axes and GPIO buttons that have physical hardware connected. Floating pins read electrical noise and cause phantom inputs.

---

## Serial Protocol

Connect at **115200 baud** (8N1). Commands are newline-terminated text.

| Command | Response | Description |
|---------|----------|-------------|
| `ping` | `{"type":"pong"}` | Connection check |
| `read_config` | `{"type":"config","data":{…}}` | Full device config as JSON |
| `write_config:{JSON}` | `{"type":"status","success":true/false,"message":"…"}` | Write + save config to flash |
| `stream_buttons` | `{"type":"buttons","data":{"states":[b0..b7]}}` repeated at 20 Hz | Start button state streaming |
| `stop_stream` | `{"type":"status","success":true}` | Stop button streaming |
| `C` | text | Enter config mode (disable HID) |
| `H` | text | Enable HID mode |
| `?` | text | Print help |

---

## Architecture

```
firmware/seedjoy/
├── seedjoy.ino         Main sketch: setup, loop, mode select, serial commands
├── config.h            DeviceConfig struct (axes, buttons, SR, BLE, power)
├── axes.cpp/h          ADC read, calibration, deadzone, curves, smoothing
├── buttons.cpp/h       GPIO debounce + SR read, logical mapping, 64-bit state
├── shift_registers.cpp/h  74HC165 SPI-style bit-bang reader
├── usb_hid.cpp/h       TinyUSB HID descriptor (4 axes, 64 buttons)
├── ble_hid.cpp/h       Bluefruit BLE HID (same descriptor)
├── ble_config.cpp/h    GATT config service: read/write/calibrate/monitor
├── storage.cpp/h       LittleFS config persistence, CRC32 validation
└── build/              Compile output (gitignored except .zip/.hex)

configurator/
├── index.html          Single-page UI (tabs: Flashing, Axes, Buttons, Advanced)
├── app.js              UI logic, tab switching, button/axis rendering
├── ble.js              WebBluetooth GATT client
├── serial.js           Web Serial client (stream_buttons, writeConfig w/ status)
├── calibration.js      Axis calibration manager
├── config.js           Config model + defaults + validation
└── styles.css          Styling + pinout image zoom
```

### HID Report Format

```
Byte 0–1   Axis 0  (int16, –32768 to 32767)
Byte 2–3   Axis 1
Byte 4–5   Axis 2
Byte 6–7   Axis 3
Byte 8     Buttons 0–7   (SR chip 1)
Byte 9     Buttons 8–15  (SR chip 2)
Byte 10    Buttons 16–23 (SR chip 3)
Byte 11    Buttons 24–31 (SR chip 4)
Byte 12    Buttons 32–39 (SR chip 5)
Byte 13    Buttons 40–47 (SR chip 6)
Byte 14    Buttons 48–55 (SR chip 7)
Byte 15    Buttons 56–63 (GPIO / unused)
```

Total: 16 bytes, descriptor: Usage Page Generic Desktop, Usage Gamepad.

---

## Troubleshooting

**Random HID input after BLE connect**
The 60-second config window should prevent this. If it still occurs, check that all enabled axes have hardware connected. Disable unused axes/buttons via **Write Config**.

**Shift register buttons not responding**
- Check D6/D7/D8 wiring and daisy-chain connections
- Verify pull-up resistors (10 kΩ on each Dn input)
- In configurator: confirm SR enabled, correct chip count, correct pins → Write Config
- Serial monitor: boot prints SR diagnostics (chip count, pins, button count)

**Write Config fails / times out**
- Always **Read Config** first before writing, to avoid overwriting good settings
- Check serial output for `{"type":"status","success":false,"message":"…"}`
- If BLE, ensure device is still in config window (send `C` to re-enter)

**Device not found in USB mode**
- Use a data-capable USB-C cable (not charge-only)
- On Windows: Device Manager → Update driver → HID-compliant game controller

**Cannot pair in BLE mode**
- Clear existing Bluetooth pairings for "SeedJoy"
- Double-tap RESET to reset bond state
- Ensure D9 is LOW at boot to select BLE mode

**Axes drifting or jittering**
- Increase deadzone in configurator
- Calibrate min/center/max with the calibration wizard
- Add 100 nF capacitor across each potentiometer output to GND

**Config not persisting after reboot**
- Check serial output for `Configuration saved to flash` or an error message
- LittleFS requires ~60 KB free in flash; firmware uses < 300 KB
- Try erasing flash and re-uploading firmware, then reconfigure

---

## Roadmap

- [x] 4 axes, 64-button HID descriptor
- [x] 74HC165 SR support — 56 buttons (7 chips), SR-only mode
- [x] USB / BLE dual mode
- [x] Web Serial + WebBluetooth configurator
- [x] Full config read/write over serial and BLE (no reboot on SR pin change)
- [x] Real-time button streaming over serial (20 Hz)
- [x] Real-time button monitoring over BLE (GATT notify)
- [x] Axis calibration wizard
- [x] Ghost-input protection (60 s config window)
- [x] LittleFS persistent config, CRC32 validation
- [ ] Firmware update via Web Serial (UF2 drag helper)
- [ ] Configuration import/export (JSON file)
- [ ] Rotary encoder support
- [ ] 8 axes via external ADC (ADS1115)
- [ ] Button matrices
- [ ] OTA firmware update via BLE DFU

---

## License

MIT — see [LICENSE](LICENSE)

## Credits

Inspired by [FreeJoy](https://github.com/FreeJoy-Team/FreeJoy) by Alexandr Yaroshenko.


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
- **Optional**: Mode selection button on D9

## Pin Configuration (Default)

### Analog Axes
- A0 (P0.02) - Axis 1 (X)
- A1 (P0.03) - Axis 2 (Y)
- A2 (P0.28) - Axis 3 (Z)
- A3 (P0.29) - Axis 4 (Rz)
### Buttons (default: shift-register mode)
- D6 (P1.11) - SR data (74HC165 QH of last chip)
- D7 (P1.12) - SR clock
- D8 (P1.13) - SR load / latch
- Up to 56 buttons via 7x 74HC165 (see the Shift Register Wiring table above)

Optional direct GPIO buttons (disabled by default in SR-only mode):
- D4 (P0.04) - GPIO button
- D5 (P0.05) - GPIO button
- D10 (P1.15) - GPIO button

### Special
- D9 (P1.14) - Mode selection (hold on boot: USB if HIGH, BLE if LOW)
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
3. The device automatically enters **Configuration Mode** for 60 seconds:
   - HID input is disabled to prevent unwanted keystrokes/mouse movements
   - **IMPORTANT**: All axes and buttons are **disabled by default** for safety
   - This prevents floating pins from generating random input
   - Hold the MODE button to stay in configuration mode
   - Send `H` via serial to enable HID immediately
4. **Read Configuration**: Click "Read from Device" to download current settings
5. **Enable Your Inputs**: 
   - **Enable only the axes/buttons you have physically connected**
   - Axes on unconnected pins will read electrical noise and cause chaos
   - Example: If you only have a potentiometer on A0, enable only Axis 0
6. **Calibrate and Configure**: 
   - Calibrate enabled axes with live preview
   - Remap buttons as needed
   - Adjust deadzones and curves
7. **Write Configuration**: Click "Write to Device" to save settings to Flash
8. Configuration is automatically saved and persists across reboots

**SAFETY WARNING**: 
- ⚠️ Only enable axes/buttons that have physical hardware connected
- Floating (unconnected) pins read random electrical noise
- This causes random keyboard/mouse input that can crash your system
- When in doubt, leave inputs disabled until you connect hardware

**Features:**
- ✅ Read/write configuration via BLE
- ✅ Real-time axis monitoring with live graphs
- ✅ Real-time button state monitoring
- ✅ Remote calibration (min/center/max)
- ✅ Battery level monitoring
- ✅ Automatic configuration mode (prevents HID interference)
- ✅ Safe defaults: all inputs disabled until explicitly enabled
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

##Random keyboard/mouse input when connecting via Bluetooth**
- This happens when axes/buttons are enabled but not physically connected
- Floating pins read electrical noise and generate random HID input
- **Solution**: In configurator, disable all unused axes and buttons
- Only enable inputs that have actual hardware connected
- Write the config to device to save the safe configuration

** Troubleshooting

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
