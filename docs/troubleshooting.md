# SeedJoy Troubleshooting Guide

## Configuration Mode Issues

### Issue: Unwanted Keystrokes/Mouse Movements During BLE Configuration

**Problem:**
When connecting to SeedJoy via Bluetooth using the web configurator, you may experience:
- Random mouse movements
- Unwanted keystrokes
- Microphone permission dialog appearing
- System behaving as if a gamepad is active

**Root Cause:**
The device advertises itself as a Bluetooth HID (Human Interface Device) gamepad and immediately starts sending HID reports upon connection. If analog input pins are floating (not connected to potentiometers) or have electrical noise, they produce random values that the operating system interprets as joystick movements or button presses.

**Solution:**
The firmware now includes a **Configuration Mode** that automatically disables HID input reporting for 10 seconds after a BLE connection is established. This gives you time to safely connect the configurator without triggering unwanted HID events.

### Configuration Mode Behavior

1. **Automatic Activation**: When a device connects via BLE, Configuration Mode is automatically enabled for 10 seconds
2. **HID Reports Disabled**: During this time, no HID gamepad reports are sent to the host computer
3. **Visual Feedback**: The configurator displays a notification explaining that Configuration Mode is active
4. **Auto-Enable**: After 10 seconds, HID reporting automatically resumes unless:
   - The MODE button is held down (keeps Configuration Mode active)
   - Serial command `C` is sent (resets the 10-second timer)

### Manual Control

You can manually control Configuration Mode using these methods:

#### Serial Commands (via USB Serial Monitor)
- **`C`** - Enter Configuration Mode (disable HID, reset timeout)
- **`H`** - Enable HID Mode immediately
- **`?`** - Show help menu

#### Hardware Button
- **Hold MODE button** - Keeps device in Configuration Mode as long as button is pressed

### Usage Tips

1. **First Connection**: Simply connect via the web configurator - Configuration Mode activates automatically
2. **Extended Configuration**: If you need more than 10 seconds, hold the MODE button during configuration
3. **Quick Enable**: To enable HID immediately after connecting, send `H` via serial or wait for timeout
4. **Preventing Issues**: Always connect to the configurator first before using the device as a gamepad

### Debug Serial Output

When connected via BLE, the serial monitor will show:
```
=======================================
BLE CONNECTED - Configuration Mode Active
HID reports disabled for 10 seconds
Hold MODE button or send 'C' via serial to stay in config mode
Send 'H' to enable HID mode immediately
=======================================
```

### Technical Details

The fix implements:
- `hidEnabled_` flag in `BLEHIDController` class
- `setHIDEnabled()` method to control HID report transmission
- Automatic zero-report sent when disabling HID (clears any stuck inputs)
- Timer-based auto-enable after 10 seconds
- Button and serial command overrides

### Related Files
- Firmware: `firmware/seedjoy/ble_hid.cpp`, `firmware/seedjoy/ble_hid.h`, `firmware/seedjoy/seedjoy.ino`
- Configurator: `configurator/app.js`

## Other Common Issues

### Device Not Showing in Bluetooth Scan
- Ensure `DEV_FORCE_BLE_MODE` is set to `1` in `seedjoy.ino` (for development)
- Check that MODE select pin is LOW (pull to ground)
- Verify the device is powered on and STATUS LED is visible
- Some browsers require HTTPS for Web Bluetooth - use Chrome/Edge

### Configuration Not Saving
- Configuration service is not yet fully implemented in firmware (v0.1.0)
- Current workaround: Settings are saved in browser's localStorage
- Future updates will add persistent storage via BLE characteristic writes

### Battery Level Not Updating
- Battery service requires LiPo battery connected to charging circuit
- Reading shows 100% if USB powered without battery
- Check voltage on VBAT pin for accurate reading

## Getting Help

If you encounter issues not covered here:
1. Check serial monitor output for error messages
2. Verify firmware version matches configurator version
3. Review hardware connections (especially analog inputs)
4. Submit issue on GitHub with serial output logs
