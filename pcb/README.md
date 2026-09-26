# SeedJoy Test Board rev A: bring-up checklist

Board: `seedjoy_testboard.kicad_pro` (KiCad 10). Fab files: `seedjoy_testboard_gerbers.zip`.
Parts: [BOM.md](BOM.md), with jumper settings.

## Before ordering the PCB

- [ ] Buy the KY-023 / PS2 joystick module first. Desolder the stick and measure it: the 3 pins of
      each pot are **2.5 mm apart**, and the two pot rows are **8.73 mm from the stick centre**
      (Alps RKJXV1224005 pattern). If your clone differs, stop and report it before ordering.

## Before powering up (bare board, multimeter)

- [ ] **3V3 to GND is not a short**: > 1 kΩ on the J2 3V3 pin vs GND.
- [ ] No XIAO fitted yet. Check continuity **J2 pin 3 (3V3) → U1 socket pin 16** and
      **J2 pin 2 (GND) → U1 socket pin 8**.

## Assembly order

1. IC socket (notch towards the XIAO side of the silkscreen), RN1 (dot = pin 1 = square pad), R1, C1.
2. DIP switch (switch 1 next to the "1 D4" label), 8 tactile switches.
3. Female headers: J1/J2 (XIAO), J3 (external analog). Male headers: J4, J5, J6, J7, J8, J9.
4. Thumbstick U2 last. Insert the **74HC165** into its socket, notch matching.
5. Plug in the XIAO **with USB-C towards the top edge** ("USB up" silkscreen).

## First power-up (no jumpers fitted, all DIP switches OFF)

1. Flash firmware: double-tap RESET → `XIAO-BOOT` drive → copy `firmware/seedjoy/build/seedjoy.uf2`.
2. After about 1 s the **blue LED is on** and the red LED is off (USB HID mounted). A red LED
   blinking 5× means USB HID failed.
3. The OS lists a game controller named **SeedJoy** (`joy.cpl` on Windows, `jstest` on Linux).
4. The serial port drops and reappears once at boot (USB re-enumeration), so a terminal usually
   misses the boot log. Open it afterwards and send `?`: the command help confirms the firmware
   is alive.

## Configure (configurator → USB Serial → Connect → **Read from Device**)

- [ ] **Pin Config → Number of chips = 1**, Invert Logic **ticked** → **Write to Device**.
      (The firmware default is 7 chips. With 7, this board's buttons show up as HID 49–56.)
- [ ] **Button Mapping** tab: BTN1…BTN8 light up as indicators 1…8. Idle = all dark.

## Analog

1. Fit **J8 on 1-2** (X from the on-board stick) and **J4 on row 1** (X → A0).
   Fit **J9 on 1-2** and **J5 on row 2** (Y → A1). *Never put J4 and J5 on the same row.*
2. Pin Config: enable **Axis 0** (pin A0) and **Axis 1** (pin A1) → Write.
3. Axis Calibration: centre raw ≈ 2048 at rest, ≈ 0 / 4095 at the ends (ratiometric ADC).
   Run min/center/max calibration → Write.
4. External sensor: move J8 (or J9) to **2-3**, plug the sensor into **J3** (GND, 3V3, X, Y, SW).
   **3.3 V only.** Use a 3.3 V-capable hall sensor (e.g. SS49E / DRV5055A1 at 3.3 V).
5. **J6 on** means the stick push button (on-board or J3 SW) acts as BTN8.

## GPIO buttons and mode (DIP switch SW9)

- [ ] Enable GPIO buttons 0–2 (pins D4, D5, D10) in the configurator → Write. DIP 1–3 ON = pressed.
      **While any GPIO button is enabled, the shift-register buttons move to HID 17+.**
- [ ] **DIP 4 ON + reset** boots BLE mode (`Selected mode: BLE`). Pair "SeedJoy" from the OS.
      Input starts **60 s after each connection** (send `H` over serial to skip).
      DIP 4 back OFF + reset returns to USB mode.

## Chaining your own 74HC165 board (J7)

J7 = 3V3, GND, CLK (D7), LOAD (D8), SER-in. Your chain's last QH → J7 pin 5.
Your N chips become HID 1…8N and this board's buttons follow. Set Number of chips = N + 1.

## Known cosmetic warnings in KiCad

The DRC shows `lib_footprint_mismatch` (library metadata) and 3 "field differs" parity notes.
They don't affect fabrication. Tools → Update Footprints from Library / Update PCB from
Schematic clears them.
