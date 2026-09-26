# SeedJoy

Game-controller firmware for the **Seeed Studio XIAO nRF52840**, with a zero-install web
configurator. The board shows up as a standard HID **joystick** over **USB** or **Bluetooth LE**,
with up to **56 buttons** (74HC165 shift registers) and **4 analog axes**.

> **Board:** Seeed Studio XIAO nRF52840, the *non-Sense* board, using the **"Seeed nRF52 Boards"**
> Arduino core (FQBN `Seeeduino:nrf52:xiaonRF52840`). The mbed-based Seeed core and the Sense
> board definition are not supported by this build.

---

## Features

- **56 buttons** from up to 7 daisy-chained 74HC165s on 3 pins (D6/D7/D8), debounced
- **4 analog axes** (A0–A3, 12-bit) with calibration, deadzone, expo curve, smoothing, invert
- **USB HID or BLE HID**, chosen at boot by pin D9
- **Web configurator** (Chrome/Edge): connect over **Web Serial** (USB cable, either mode) or
  **WebBluetooth** (BLE mode), read and write the full config, calibrate axes, and watch live
  button states
- **Persistent config** in on-chip flash (LittleFS, CRC32-checked)
- **Ghost-input protection**: HID input is held off for 60 s after each BLE connection
- **Battery level** reported through the BLE Battery Service (LiPo on the XIAO's BAT pads)
- Force feedback (USB PID) exists as an experimental, **off-by-default** build option, see
  [docs/ffb-plan.md](docs/ffb-plan.md)

---

## Hardware

### Parts

| Part | Qty | Notes |
|------|-----|-------|
| Seeed Studio XIAO nRF52840 | 1 | Non-Sense |
| 74HC165 shift register | 1–7 | 8 buttons each |
| 10 kΩ resistor | 1 per button | Pull-up on each 74HC165 input |
| Buttons / switches | up to 56 | Input → GND |
| 10 kΩ linear potentiometer | 0–4 | Optional axes |
| Switch or jumper on D9 | 1 | Only to boot into BLE mode |
| 3.7 V single-cell LiPo | 0–1 | Optional, for wireless use |

**Everything runs at 3.3 V.** Power the 74HC165s, pull-ups and pots from the XIAO's **3V3** pin.
XIAO pins are not 5 V tolerant.

### Pin map

| XIAO pin | Port | Function |
|----------|------|----------|
| A0–A3 (D0–D3) | P0.02, P0.03, P0.28, P0.29 | Axes 0–3 (X, Y, Z, Rz), **disabled by default** |
| D4, D5, D10 | P0.04, P0.05, P1.15 | Optional direct GPIO buttons, disabled by default |
| **D6** | P1.11 | 74HC165 **data**: QH (pin 9) of the chip nearest the XIAO |
| **D7** | P1.12 | 74HC165 **clock**: CLK (pin 2) of every chip |
| **D8** | P1.13 | 74HC165 **load**: SH/LD (pin 1) of every chip |
| **D9** | P1.14 | **Mode select**: open = USB, GND at boot = BLE |
| RGB LED | P0.26 red, P0.06 blue | Red: booting / error blinks. Blue: USB mounted, or BLE connected |
| BAT+/BAT− pads | n/a | LiPo (underside) |

### Shift-register wiring (74HC165)

```
            far end                                          nearest XIAO
 3V3 → SER[Chip 1]QH → SER[Chip 2]QH → … → SER[Chip 7]QH ──────────► D6
        buttons 1–8    buttons 9–16          buttons 49–56

 D7 → CLK (pin 2) of every chip        D8 → SH/LD (pin 1) of every chip
 3V3 → VCC (16) of every chip           GND → GND (8) and CLK INH (15) of every chip
```

- Chain with **QH (pin 9) → next chip's SER (pin 10)**. Do not use /QH (pin 7); it's inverted.
- The far-end chip's SER goes to **3V3** (so unused chain positions read as released). Buttons
  1–8 are on that chip: input A = button 1, input H = button 8.
- Each input gets a **10 kΩ pull-up to 3V3**, and the switch goes from the input to **GND**.
  Released reads HIGH, pressed reads LOW. The default *Invert Logic = on* reports pressed as 1.
- Fewer than 7 chips is fine. Set **Number of chips** in the configurator to match what's fitted,
  because numbering always starts at the far-end chip.

Full wiring guide with the 74HC165 pinout: [docs/hardware.md](docs/hardware.md).

---

## Build and flash

### 1. Install the board support package

Arduino IDE 2.x or [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/).
Add this board-manager URL:

```
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
```

Install **Seeed nRF52 Boards** (tested with **1.1.10**). Do *not* install the "mbed-enabled"
package. No extra libraries are needed: TinyUSB, Bluefruit and LittleFS ship with the core.
Don't install a separate "Adafruit TinyUSB" from Library Manager, since it can shadow the
core's copy.

```bash
arduino-cli config add board_manager.additional_urls \
  https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
arduino-cli core update-index
arduino-cli core install Seeeduino:nrf52
```

### 2. Compile

```bash
cd firmware/seedjoy
./compile.sh
```

This produces `build/seedjoy.uf2` (drag-and-drop image), `build/seedjoy.ino.zip` (serial DFU
package) and `build/seedjoy.ino.hex`. `compile.sh` uses `arduino-cli` from your `PATH` (or
`~/bin/arduino-cli`).

In the Arduino IDE: open `firmware/seedjoy/seedjoy.ino`, select **Tools → Board → Seeed nRF52
Boards → Seeed XIAO nRF52840** (not "Sense"), then Upload.

### 3. Flash

**Option A: UF2 drag-and-drop (simplest)**

1. Connect the XIAO by USB-C (use a data cable).
2. **Double-tap the RESET button.** A USB drive appears (the UF2 bootloader).
3. Copy `build/seedjoy.uf2` onto that drive. The board reboots into SeedJoy.

**Option B: serial upload**

```bash
./upload.sh              # finds /dev/cu.usbmodem* (macOS) or /dev/ttyACM* (Linux)
PORT=/dev/cu.usbmodem1101 ./upload.sh   # or pick the port yourself
```

Or in the IDE, press Upload. If the port isn't found, double-tap RESET and retry.

### Apple Silicon Macs without Rosetta

Seeed's core 1.1.10 ships **x86_64-only** `arm-none-eabi-gcc` 9-2019q4 and `adafruit-nrfutil`,
and Arduino's `ctags` is x86_64-only too. Without Rosetta, compiling fails with
`bad CPU type in executable`. Either install Rosetta (`softwareupdate --install-rosetta`), or
point the scripts at native tools:

```bash
# 1. Native ARM toolchain (Arm GNU Toolchain, darwin-arm64 tarball, e.g. 13.3.rel1)
#    https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# 2. adafruit-nrfutil from PyPI
python3 -m venv ~/.venvs/nrfutil && ~/.venvs/nrfutil/bin/pip install adafruit-nrfutil
# 3. Arduino's ctags fork, built natively
git clone --depth 1 --branch 5.8-arduino11 https://github.com/arduino/ctags.git
cd ctags && grep -rl __unused__ . | grep -E '\.(c|h)$' | xargs sed -i '' 's/__unused__/CTAGS_UNUSED/g'
./configure && make && cd ..

# then:
ARM_GCC_BIN=/path/to/arm-gnu-toolchain-13.3.rel1-darwin-arm64-arm-none-eabi/bin \
NRFUTIL=~/.venvs/nrfutil/bin/adafruit-nrfutil \
CTAGS_DIR=/path/to/ctags \
./compile.sh

NRFUTIL=~/.venvs/nrfutil/bin/adafruit-nrfutil ./upload.sh   # or just use the UF2 drive
```

This is how the current tree was build-verified: gcc 13.3.rel1, 148 KB flash (18 %), 20.6 KB RAM
(8 %). The "`_close is not implemented`"-style linker warnings from newer newlib are harmless.

---

## First boot and mode selection

D9 is sampled **once at boot** (internal pull-up):

| D9 at reset | Mode | What the host sees |
|-------------|------|--------------------|
| Nothing connected | **USB** | USB joystick "SeedJoy" + a serial port (VID `0x239A`, PID `0x80F4`) |
| Tied to GND | **BLE** | BLE HID joystick advertising as "SeedJoy" (plus the serial port if USB is plugged in) |

To change mode, change D9 and press RESET. The `mode` field in the saved config is currently
informational only, and D9 always decides.

In USB mode the serial port drops once and reappears about a second after boot. That's expected:
the firmware forces the host to re-enumerate so the joystick interface appears.

Boot log (Serial Monitor, 115200 baud) prints the selected mode, enabled axes and the
shift-register setup.

### Default configuration

| Setting | Default |
|---------|---------|
| Shift registers | **enabled**, 7 chips, D6/D7/D8, Invert Logic **on** |
| Axes 0–3 | **disabled** (enable only the ones you've wired) |
| Direct GPIO buttons | disabled |
| Device name | `SeedJoy` |

With a full 7-chip chain, a board wired as above works with no configuration at all: SR buttons
appear as HID buttons 1–56. With fewer chips, the fitted chips first show up at the top of the
range (one chip on D6 = buttons 49–56) until you set *Number of chips* in the configurator. Nothing
reads as stuck pressed, as long as the far-end SER is tied to 3V3.

> **Upgrading from an older build?** A config already saved on the board keeps its old values.
> In particular the old default was *Invert Logic = off*, which makes every idle button read as
> pressed with pull-up wiring. Open the configurator, **Read from Device**, tick **Invert Logic**,
> and **Write to Device**.

### Testing it

- **Windows:** `joy.cpl` (Game Controllers), select SeedJoy, then Properties
- **Linux:** `jstest /dev/input/js0`
- **Any OS:** https://hardwaretester.com/gamepad in Chrome

### BLE mode and the 60-second window

Every time a BLE central connects (your PC *or* the web configurator), the firmware holds off
HID input for **60 seconds** so floating or unconfigured inputs can't spam the host while you
configure. The blue LED lights while connected.

- After pairing with your PC, expect **no input for the first minute** of each connection.
- Send `H` over the serial port to enable HID immediately, or `C` to restart the window.
- Press and hold a button on D9 (to GND) during the window to keep it open. A switch left
  closed since boot is ignored for this, so a permanent BLE jumper won't block HID.

Pair from the OS Bluetooth settings as you would any controller. If pairing gets stuck, remove
"SeedJoy" from the OS device list and reset the board.

---

## Web configurator

### Open it

WebBluetooth and Web Serial need a secure context. `http://localhost` counts as one:

```bash
cd configurator
python3 -m http.server 8000
# open http://localhost:8000 in Chrome or Edge (Firefox/Safari have neither API)
```

### Connect

Pick **🔌 USB Serial** or **📡 Bluetooth** at the top, then **Connect Device**.

| Mode | Transport | Works when the firmware is in |
|------|-----------|-------------------------------|
| USB Serial (Web Serial, 115200 baud) | USB cable | USB mode **or** BLE mode (with cable plugged in) |
| Bluetooth (WebBluetooth) | Wireless GATT | BLE mode only |

The Bluetooth picker filters on names starting with `SeedJoy`. If you rename the device, keep
that prefix.

### Workflow

1. **Read from Device** loads the board's current config into the UI. Always do this first.
2. **Pin Config** tab: shift registers (enable, chip count, pins, Invert Logic), axis and GPIO
   button enables.
3. **Axis Calibration** tab: live readout, set min/center/max, deadzone, curve, smoothing.
4. **Button Mapping** tab: 56 indicators light up live as you press buttons. Over serial the
   firmware streams states at 20 Hz; over BLE they arrive as GATT notifications.
5. **Write to Device** saves to flash. Shift-register and axis changes apply immediately, no
   reboot needed.

Pin values in the configurator and in the JSON protocol are **Arduino pin numbers**
(D0–D10 = 0–10); see [docs/protocol.md](docs/protocol.md#pin-numbers).

> ⚠️ Enable only axes and buttons that are physically wired. A floating pin reads noise and
> produces phantom input.

---

## Serial protocol

115200 baud, newline-terminated text commands. Available in both USB and BLE mode.

| Command | Response | Purpose |
|---------|----------|---------|
| `ping` | `{"type":"pong"}` | Connection check |
| `read_config` | `{"type":"config","data":{…}}` | Full config as JSON |
| `write_config:{JSON}` | `{"type":"status","success":true\|false,"message":"…"}` | Apply and save config |
| `stream_buttons` | `{"type":"buttons","data":{"states":[b0,…,b7]}}` at 20 Hz | Start live button stream (8 bytes = 64 bits) |
| `stop_stream` | `{"type":"status",…}` | Stop the stream |
| `C` | text | BLE mode: (re)enter the config window, HID off |
| `H` | text | BLE mode: enable HID now |
| `?` | text | Help |

The BLE GATT config service and message formats are described in [docs/protocol.md](docs/protocol.md).

---

## HID report

One 16-byte input report. Usage page Generic Desktop, usage **Joystick**. Over USB it has no
report ID; over BLE it's Report ID 1.

```
Bytes 0–7    Axes X, Y, Z, Rz   int16 little-endian, −32767 … 32767
Bytes 8–15   Buttons 1–64       1 bit each (SR-only mode uses buttons 1–56)
```

Button bit *n* is HID button *n+1*. In SR-only mode, SR input *n* maps to bit *n*. If any direct
GPIO button is enabled, SR buttons move up by 16 (the 16 GPIO slots come first), so a 7-chip
chain's last 8 buttons no longer fit in the 64-bit report. For 56 buttons, use SR only.

---

## Troubleshooting

**Every button shows as pressed.** Invert Logic is off (a config saved by an older build). Read →
tick *Invert Logic* → Write.

**Buttons shifted / wrong numbers.** The configured chip count doesn't match the chips fitted, or
the chain is wired through /QH (pin 7) instead of QH (pin 9).

**No shift-register buttons at all.** Check D6/D7/D8 and 3V3/GND to every chip, CLK INH (pin 15)
to GND, and SR *enabled* in the config. The boot log prints the SR settings. Configs written by
older configurator builds stored invalid pin numbers (e.g. 43/44/45 for D6/D7/D8). The firmware
now repairs these at boot and logs `Repaired invalid pin numbers in stored config`.

**USB mode: no joystick, only a serial port.** Use a data cable, and check the boot log says
`USB HID initialized successfully`. On Windows, look in `joy.cpl`, not only Device Manager.

**BLE mode: connected but no input.** Wait out the 60 s window, or send `H` over serial.

**Can't find the device in the Bluetooth picker.** Check D9 was on GND at reset (the boot log
says `Selected mode: BLE`), and that the name still starts with `SeedJoy`.

**Axes drift or jitter.** Calibrate, raise the deadzone or smoothing, add 100 nF from wiper to GND.

**Config lost after reboot.** After a write the log should say `Config saved to Flash`; on the next boot, `Config loaded from Flash`. Anything else (`CRC mismatch`, `size mismatch`) means the stored config was rejected and defaults were used.

**Compile fails with `bad CPU type in executable`.** See
[Apple Silicon Macs without Rosetta](#apple-silicon-macs-without-rosetta).

More: [docs/troubleshooting.md](docs/troubleshooting.md).

---

## Project layout

```
firmware/seedjoy/
├── seedjoy.ino          setup/loop, D9 mode select, BLE config window, serial commands
├── config.h             DeviceConfig struct, default pins
├── axes.*               ADC read, calibration, deadzone, curves, smoothing
├── buttons.*            GPIO + shift-register buttons, debounce, 64-bit state
├── shift_registers.*    74HC165 bit-bang reader
├── usb_hid.*            TinyUSB joystick (16-byte report)
├── ble_hid.*            Bluefruit BLE HID joystick + battery service
├── ble_config.*         GATT config / monitor / calibration service
├── storage.*            LittleFS config persistence, CRC32
├── ffb_*                Experimental force feedback (ENABLE_FFB=0 by default)
├── compile.sh / upload.sh
firmware/tests/          Host-side unit tests for the FFB engine (bash run_tests.sh)
configurator/            Static web app: index.html, app.js, serial.js, ble.js, config.js, …
docs/                    hardware.md, protocol.md, troubleshooting.md, getting-started.md, FFB docs
```

---

## Status and known limitations

- The current tree **compiles** for `Seeeduino:nrf52:xiaonRF52840` (core 1.1.10) and the host
  unit tests pass. The fixes in this revision (USB re-enumeration, LED polarity, D9 handling,
  configurator pin numbering, SR polarity default, stored-pin repair) have **not yet been verified on a physical
  board**.
- BLE config characteristics are unauthenticated (anyone in range can write config). See
  [docs/audit.md](docs/audit.md) H2.
- `mode` in the config is ignored (D9 decides), and `autoSleep` isn't implemented yet.
- Mixing GPIO buttons with a full 7-chip chain drops the last 8 SR buttons (see HID report).

## Roadmap

- [ ] Configuration import/export as a JSON file
- [ ] Rotary encoders
- [ ] 8 axes via external ADC (ADS1115)
- [ ] Button matrices
- [ ] OTA firmware update via BLE DFU

## License

MIT, see [LICENSE](LICENSE).

## Credits

Inspired by [FreeJoy](https://github.com/FreeJoy-Team/FreeJoy) by Alexandr Yaroshenko.
