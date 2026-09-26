# Getting Started with SeedJoy

A step-by-step path from a bare **Seeed Studio XIAO nRF52840** (non-Sense) to a working
controller. The [README](../README.md) is the reference for every detail mentioned here.

## 1. What you need

- Seeed Studio XIAO nRF52840 (the non-Sense board)
- USB-C **data** cable
- 1–7× 74HC165, a 10 kΩ pull-up per button, and the buttons
- Optional: up to 4× 10 kΩ potentiometers, a switch or jumper for D9, a 3.7 V LiPo
- Chrome or Edge for the configurator

## 2. Wire it

Follow [hardware.md](hardware.md). The minimum working setup:

- **One 74HC165**: QH (pin 9) → **D6**, CLK (pin 2) → **D7**, SH/LD (pin 1) → **D8**,
  VCC → **3V3**, GND and CLK INH (pin 15) → GND, SER (pin 10) → **3V3**
- Inputs A–H each with a 10 kΩ pull-up to 3V3 and a button to GND
- Nothing on D9 (boots in USB mode)

## 3. Install the board package

Arduino IDE 2.x: **File → Preferences → Additional Boards Manager URLs**:

```
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
```

Then **Boards Manager → "Seeed nRF52 Boards"** → Install (tested: 1.1.10). Don't use the
*mbed-enabled* package, and don't install extra libraries: TinyUSB, Bluefruit and LittleFS ship
with the core.

## 4. Build and flash

**Arduino IDE:** open `firmware/seedjoy/seedjoy.ino`, choose **Tools → Board → Seeed nRF52
Boards → Seeed XIAO nRF52840**, pick the port, press **Upload**.

**Command line:**

```bash
cd firmware/seedjoy
./compile.sh                    # → build/seedjoy.uf2, .zip, .hex
```

then either `./upload.sh`, or **double-tap RESET** on the XIAO and copy `build/seedjoy.uf2` onto
the USB drive that appears.

On an Apple Silicon Mac without Rosetta, see the README section
"Apple Silicon Macs without Rosetta".

## 5. Check the boot log

Serial Monitor at **115200** baud, then press RESET. The port disappears for a moment around
1 s after boot in USB mode, which is expected. You should see lines like:

```
SeedJoy - BLE/USB Game Controller
Config loaded from Flash            (first boot: "Using default configuration")
Mode select pin: USB (override)
Selected mode: USB
Shift Registers:
  Chips  : 7
...
USB HID initialized successfully
Setup complete!
```

## 6. Set the chip count, then test

1. `cd configurator && python3 -m http.server 8000`, then open **http://localhost:8000**.
2. Choose **🔌 USB Serial** → **Connect Device** → select the XIAO's port.
3. **Read from Device**.
4. **Pin Config**: set *Number of chips* to what you fitted and keep *Invert Logic* ticked.
   **Write to Device**.
5. **Button Mapping**: press buttons and watch the indicators.
6. Check the OS sees a joystick: `joy.cpl` on Windows, `jstest /dev/input/js0` on Linux, or
   https://hardwaretester.com/gamepad.

Axes: wire a pot to A0, enable **Axis 0**, write, then calibrate in **Axis Calibration**.

## 7. Wireless (BLE)

1. Connect D9 to GND (switch or jumper) and press RESET. The log says `Selected mode: BLE`.
2. Pair "SeedJoy" from the OS Bluetooth settings.
3. Input starts **60 s after each connection** (ghost-input protection). Send `H` over serial to
   skip the wait.
4. The configurator's **📡 Bluetooth** mode works in BLE mode too.

## Next

- [hardware.md](hardware.md): full wiring, numbering, battery
- [protocol.md](protocol.md): serial and BLE config protocol
- [troubleshooting.md](troubleshooting.md)
