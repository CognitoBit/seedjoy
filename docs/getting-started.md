# Getting Started with SeedJoy

This guide will help you build and use your SeedJoy game controller.

## Prerequisites

### Hardware
- Seeed Studio XIAO nRF52840 (Sense or standard version)
- USB-C cable (must support data, not charge-only)
- 4x 10kΩ potentiometers (for analog axes)
- Up to 16x tactile buttons or switches
- Optional: 3.7V LiPo battery (100-500mAh)
- Breadboard and jumper wires

### Software
- Arduino IDE 2.0+ or Arduino CLI
- Chrome, Edge, or Opera browser (for web configurator)

## Step 1: Hardware Assembly

Follow the [Hardware Wiring Guide](hardware.md) to connect your potentiometers and buttons to the XIAO.

**Minimal setup for testing:**
- 2 potentiometers on A0 and A1
- 4 buttons on D1, D2, D3, D4
- Mode select switch on D0 (or jumper wire)

## Step 2: Install Arduino IDE

1. Download [Arduino IDE 2.0+](https://www.arduino.cc/en/software)
2. Install and launch Arduino IDE

## Step 3: Add Board Support

1. Open Arduino IDE
2. Go to **File → Preferences**
3. In "Additional Boards Manager URLs", add:
   ```
   https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
   ```
4. Click **OK**
5. Go to **Tools → Board → Boards Manager**
6. Search for "Seeed nRF52 Boards"
7. Install **Seeed nRF52 Boards** (or **Seeed nRF52 mbed-enabled Boards**)
8. Wait for installation to complete

## Step 4: Install Required Libraries

1. Go to **Sketch → Include Library → Manage Libraries**
2. Install these libraries:
   - **Adafruit TinyUSB Library**
   - **Adafruit nRFCrypto**
   - **Adafruit LittleFS**
   - **Bluefruit nRF52 Libraries** (should be included with board package)

## Step 5: Open SeedJoy Firmware

1. Navigate to the downloaded/cloned SeedJoy repository
2. Open `firmware/seedjoy/seedjoy.ino` in Arduino IDE

## Step 6: Select Board and Port

1. Go to **Tools → Board**
2. Select **Seeed nRF52 Boards → Seeed XIAO nRF52840**
   - Or **Seeed XIAO nRF52840 Sense** if you have the Sense version
3. Connect XIAO to your computer via USB-C
4. Go to **Tools → Port**
5. Select the port that appears (e.g., `/dev/cu.usbmodem1234` on Mac, `COM3` on Windows)

## Step 7: Upload Firmware

1. Click the **Upload** button (→ icon) in Arduino IDE
2. Wait for compilation and upload (may take 1-2 minutes first time)
3. You should see "Done uploading" when complete

**If upload fails:**
- Double-click the RESET button on XIAO to enter bootloader mode
- XIAO should appear as a USB drive called "XIAO-SENSE" or "XIAO"
- Try uploading again immediately

## Step 8: Verify Operation

1. Open **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. Press RESET button on XIAO
4. You should see boot messages:

```
=================================
SeedJoy - BLE/USB Game Controller
Firmware v0.1.0
=================================
Initializing storage...
Loading configuration...
Config loaded from Flash
Device name: SeedJoy
Config mode: USB
Selected mode: USB
Starting USB HID...
USB HID initialized successfully
Setup complete!
=================================
```

## Step 9: Test in Operating System

### Windows

1. Open **Control Panel** → **Devices and Printers**
2. Right-click on **SeedJoy Controller**
3. Select **Game Controller Settings**
4. Click **Properties**
5. Move your axes and press buttons - you should see them respond

Alternatively, use [HTML5 Gamepad Tester](https://gamepad-tester.com/)

### macOS

1. Download [Joystick Show](https://github.com/alvarorahul/JoystickShow)
2. Open the app
3. Your SeedJoy should appear in the device list
4. Test axes and buttons

### Linux

Install joystick tools:
```bash
sudo apt-get install joystick
```

Test the controller:
```bash
jstest /dev/input/js0
```

Or use GUI:
```bash
sudo apt-get install jstest-gtk
jstest-gtk
```

## Step 10: Configure via Web Interface

### USB Mode Configuration

Currently in MVP, configuration is primarily for BLE. For USB mode:
- Default calibration should work for most potentiometers
- Button mapping is 1:1 by default
- Adjust settings via serial commands if needed (see protocol.md)

### BLE Mode Configuration

1. Set mode select switch to BLE (D0 → GND) or change in config
2. Reset XIAO
3. Open Chrome/Edge/Opera browser
4. Navigate to the configurator (see configurator README)
5. Click **Connect Device**
6. Select **SeedJoy-XXXX** from the list
7. Configure settings:
   - Calibrate axes
   - Remap buttons
   - Adjust deadzone and curves
   - Set BLE parameters
8. Click **Write to Device** to save

## Step 11: Calibration

For best axis accuracy:

1. Open web configurator and connect
2. Go to **Axis Calibration** tab
3. For each axis:
   - Move axis to minimum position → Click **Set Minimum**
   - Move axis to center position → Click **Set Center**
   - Move axis to maximum position → Click **Set Maximum**
4. Adjust deadzone if needed (5% is good default)
5. Click **Write to Device**

## Switching Between USB and BLE Modes

### Manual Mode Selection (Default)

Use the mode select button/switch on D0:
- **D0 HIGH (open)** → USB mode
- **D0 LOW (to GND)** → BLE mode

The mode is checked on boot, so you need to RESET after changing.

### Software Mode Selection

1. Connect via web configurator (in BLE mode)
2. Go to **Advanced** tab
3. Change **Operation Mode**:
   - USB: Always use USB HID
   - BLE: Always use BLE HID
   - Auto: Use USB if connected, otherwise BLE (future)
4. Click **Write to Device**
5. Reset XIAO

## Wireless Operation (BLE Mode)

1. Connect LiPo battery to XIAO
2. Set mode to BLE
3. Power on (disconnect USB if connected)
4. Pair in OS Bluetooth settings:
   - Windows: Settings → Bluetooth & devices → Add device
   - macOS: System Settings → Bluetooth
   - Linux: bluetoothctl → scan on → pair [address]
5. Once paired, device appears as HID gamepad
6. Test in game/flight simulator

**Battery Life:**
- ~8-12 hours with 500mAh battery at 7.5ms connection interval
- ~15-20 hours with power-saving settings
- Recharge via USB-C (charging works even while in use)

## Troubleshooting

### "upload: No device found on COM3"
→ Double-tap RESET button, then upload immediately

### "USB device not recognized"
→ Check USB cable supports data, try different port, update drivers

### "Axes are jittery"
→ Add 0.1µF capacitors across potentiometers, increase smoothing, check wiring

### "Buttons not responding"
→ Check button wiring (should connect pin to GND), verify in serial monitor

### "Can't pair in BLE mode"
→ Ensure BLE mode selected, check OS Bluetooth settings, unpair and re-pair

### "Configuration doesn't save"
→ Check serial monitor for errors, try "Reset to Defaults" then reconfigure

### "Battery drains quickly"
→ Increase connection interval, enable auto-sleep, check for current leaks in wiring

## Next Steps

- **Customize button layout** for your flight sim/racing game
- **Create axis curves** for non-linear response (expo for precise aiming)
- **Add encoders** (future enhancement) for rotary controls
- **Build enclosure** for your controller (3D print or custom case)
- **Share your build** on GitHub Discussions!

## Support

- **Documentation:** [docs/](../docs/)
- **Issues:** [GitHub Issues](https://github.com/yourusername/seedjoy/issues)
- **Community:** Discord (link TBD)

## Advanced Topics

- [Configuration Protocol](protocol.md) - Serial/BLE config commands
- [Firmware Development](../firmware/README.md) - Modifying the firmware
- [Web Configurator Development](../configurator/README.md) - Customizing the UI

Enjoy your SeedJoy controller! 🎮
