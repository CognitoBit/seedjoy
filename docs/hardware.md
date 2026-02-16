# Hardware Wiring Guide

This guide shows how to wire potentiometers and buttons to your Seeed Studio XIAO nRF52840.

## Required Components

- **Seeed Studio XIAO nRF52840** (1x)
- **10kΩ Potentiometers** (4x for analog axes)
- **Tactile buttons or switches** (up to 16x)
- **Optional: LiPo battery** (3.7V, 100-500mAh for wireless operation)
- **Breadboard and jumper wires**

## Pinout Reference

### XIAO nRF52840 Pinout

```
         ┌─────────┐
    3V3  │1      14│ GND
    GND  │2      13│ 3V3
   D0/P0.02(A0)  12│ P0.03(A1)/D1
   D1/P0.29(A3)  11│ P0.28(A2)/D2
   D2/P0.04   10│ P0.05/D3
   D3/P0.06    9│ P0.07/D4
   D4/P0.08    8│ P0.09/D5
   D5/P0.10    7│ P0.11/D6
   D6/P0.12    6│ P0.13/D7
         └─────────┘
         USB-C Port
```

### Default Pin Assignment

**Analog Axes:**
- A0 (P0.02) → Axis 1 (X)
- A1 (P0.03) → Axis 2 (Y)
- A2 (P0.28) → Axis 3 (Z)
- A3 (P0.29) → Axis 4 (Rz/Throttle)

**Digital Buttons:**
- D1-D10 → Buttons 1-10
- MOSI, MISO, SCK → Buttons 11-13
- TX, RX, SCL → Buttons 14-16

**Special:**
- D0 → Mode selection (USB/BLE)
- LED_RED → Status indicator
- LED_BLUE → Connection indicator

## Wiring Potentiometers (Analog Axes)

Potentiometers have 3 pins:
1. **Pin 1** (left) → GND
2. **Pin 2** (center/wiper) → Analog pin (A0-A3)
3. **Pin 3** (right) → 3V3

### Example: Axis 1 (X)

```
Potentiometer
  Pin 1 ──────┐
  Pin 2 ────┐ │
  Pin 3 ──┐ │ │
          │ │ │
XIAO      │ │ │
  3V3 ────┘ │ │
  A0 ───────┘ │
  GND ────────┘
```

Repeat for A1, A2, A3 for additional axes.

### Capacitor for Noise Reduction (Optional)

Add a 0.1µF ceramic capacitor between each analog pin and GND to reduce noise:

```
       ┌─────────┐
A0 ────┤ 0.1µF  ├──── GND
       └─────────┘
```

## Wiring Buttons

Buttons are wired between digital pins and GND (INPUT_PULLUP mode).

### Simple Button

```
Button
  Pin 1 ────────┐
  Pin 2 ──┐     │
          │     │
XIAO      │     │
  D1 ─────┘     │
  GND ──────────┘
```

The firmware enables internal pull-up resistors, so no external resistor is needed.

### Button Matrix (Future Enhancement)

For >16 buttons, use a button matrix:

```
         Col1   Col2   Col3   Col4
Row1 ────┬──────┬──────┬──────┬────
         │      │      │      │
        BTN1   BTN2   BTN3   BTN4
Row2 ────┬──────┬──────┬──────┬────
         │      │      │      │
        BTN5   BTN6   BTN7   BTN8
```

## Mode Selection Button

Wire a button or toggle switch to D0:
- **Open (HIGH)** → USB mode
- **Closed (LOW)** → BLE mode

```
Toggle Switch (SPST)
  
  Position 1 (USB)     Position 2 (BLE)
  ───────┬────         ────┬─────────
         │                 │
XIAO     │                 │
  D0 ────┘                 └──── GND
```

## Battery Connection (Optional)

For wireless operation, connect a LiPo battery:

1. **With JST connector:**
   - Connect JST battery directly to XIAO's battery pads
   
2. **Manual wiring:**
   - Battery + → VBAT/BAT+
   - Battery - → GND

**Important:** 
- Use 3.7V LiPo only (max 4.2V when charged)
- XIAO has built-in charging circuit when USB is connected
- Do NOT exceed 500mA battery capacity without external protection

## Complete Wiring Example

### Minimal Setup (2 Axes, 4 Buttons)

```
Components:
- 2x 10kΩ potentiometers (X and Y axes)
- 4x tactile buttons
- 1x mode select switch

Wiring:
Pot 1: GND, A0, 3V3
Pot 2: GND, A1, 3V3
Button 1: D1, GND
Button 2: D2, GND
Button 3: D3, GND
Button 4: D4, GND
Mode Switch: D0, GND (for BLE)
```

### Full Setup (4 Axes, 16 Buttons)

```
Components:
- 4x 10kΩ potentiometers
- 16x tactile buttons
- 1x mode select switch
- 4x 0.1µF capacitors (optional, for noise reduction)
- 1x 3.7V LiPo battery (optional)

Wiring:
Axes:
  Pot 1: GND, A0, 3V3 (+ 0.1µF cap)
  Pot 2: GND, A1, 3V3 (+ 0.1µF cap)
  Pot 3: GND, A2, 3V3 (+ 0.1µF cap)
  Pot 4: GND, A3, 3V3 (+ 0.1µF cap)

Buttons:
  BTN 1-10: D1-D10 → GND
  BTN 11: MOSI → GND
  BTN 12: MISO → GND
  BTN 13: SCK → GND
  BTN 14: TX → GND
  BTN 15: RX → GND
  BTN 16: SCL → GND

Mode: D0 → GND (via switch)
Battery: BAT+ and GND
```

## Troubleshooting

**Axes jittering or noisy:**
- Add 0.1µF capacitors across analog inputs
- Use shielded cable for potentiometers
- Keep analog wires away from power/digital lines
- Use higher quality potentiometers

**Buttons not responding:**
- Check wiring (should be pin → GND)
- Verify button is making contact
- Try a different pin to rule out hardware issue

**USB not recognized:**
- Check USB cable (must support data)
- Try different USB port
- Verify firmware uploaded correctly
- Check serial monitor for boot messages

**BLE not connecting:**
- Ensure BLE mode selected (D0 → GND)
- Check pairing in OS Bluetooth settings
- Move closer to PC (within 10m)
- Check if BLE HID is enabled in OS

## Safety Notes

⚠️ **Warnings:**
- Never connect voltages >3.3V to any pin
- Do not reverse polarity on battery
- Do not short 3V3 to GND
- Use appropriate fuse for battery (recommended)
- Keep battery away from sharp objects/heat

## Next Steps

After wiring:
1. Flash firmware to XIAO (see README.md)
2. Test axes and buttons in serial monitor
3. Calibrate axes using web configurator
4. Test in Windows Game Controllers or jstest (Linux)
5. Configure button mappings as needed

## Additional Resources

- [XIAO nRF52840 Wiki](https://wiki.seeedstudio.com/XIAO_BLE/)
- [Potentiometer Basics](https://learn.sparkfun.com/tutorials/voltage-dividers)
- [Button Matrix Design](https://www.baldengineer.com/arduino-keyboard-matrix-tutorial.html)
