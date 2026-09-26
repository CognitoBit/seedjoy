# Hardware Wiring Guide

How to wire SeedJoy on a **Seeed Studio XIAO nRF52840** (the non-Sense board; the Sense
variant uses a different Arduino board definition and is not what the build targets).

Everything on the XIAO is **3.3 V logic. No pin is 5 V tolerant.** Power every external
chip and potentiometer from the XIAO's **3V3** pin, never from 5V.

## Components

| Part | Qty | Notes |
|------|-----|-------|
| Seeed Studio XIAO nRF52840 | 1 | Non-Sense |
| 74HC165 (8-bit parallel-in / serial-out) | 1–7 | 8 buttons per chip, 56 max |
| 10 kΩ resistor | 1 per button | Pull-up on each 74HC165 input |
| Push buttons / switches | up to 56 | Wired input → GND |
| 10 kΩ linear potentiometer | 0–4 | Analog axes on A0–A3 |
| Switch or jumper on D9 | 1 | Only needed to boot into BLE mode |
| 100 nF ceramic capacitor | optional | One per 74HC165 (VCC–GND), one per pot wiper (to GND) |
| 3.7 V LiPo | optional | Wireless (BLE) use |

## XIAO nRF52840 pins used by SeedJoy

Physical layout, board seen from the top with the USB-C connector pointing up:

```
            USB-C
       ┌─────────────┐
  D0/A0│●           ●│5V
  D1/A1│●           ●│GND
  D2/A2│●           ●│3V3
  D3/A3│●           ●│D10
     D4│●           ●│D9
     D5│●           ●│D8
     D6│●           ●│D7
       └─────────────┘
```

| XIAO pin | nRF52840 port | SeedJoy function (default) |
|----------|---------------|----------------------------|
| A0 / D0  | P0.02 | Axis 0 (X), disabled until enabled in the configurator |
| A1 / D1  | P0.03 | Axis 1 (Y), disabled until enabled |
| A2 / D2  | P0.28 | Axis 2 (Z), disabled until enabled |
| A3 / D3  | P0.29 | Axis 3 (Rz), disabled until enabled |
| D4       | P0.04 | Optional direct GPIO button (disabled) |
| D5       | P0.05 | Optional direct GPIO button (disabled) |
| D6       | P1.11 | **74HC165 serial data in** (QH, pin 9, of the chip nearest the XIAO) |
| D7       | P1.12 | **74HC165 clock** (CLK, pin 2, all chips) |
| D8       | P1.13 | **74HC165 load** (SH/LD, pin 1, all chips) |
| D9       | P1.14 | **Mode select** (see below) |
| D10      | P1.15 | Optional direct GPIO button (disabled) |
| 3V3      | n/a | Power for 74HC165s, pull-ups, pots |
| GND      | n/a | Common ground |
| On-board RGB LED | P0.26 red / P0.06 blue | Red = booting / error blink, blue = USB mounted or BLE connected |
| BAT+ / BAT− pads (underside) | n/a | LiPo |

The firmware uses Arduino pin numbers (`D6` = 6, `A0` = 0, and so on). The configurator's pin
drop-downs use the same numbers.

## Mode select (D9)

D9 is read **once at boot** with the internal pull-up enabled:

| D9 at power-up / reset | Mode |
|------------------------|------|
| Not connected (floating → pulled HIGH) | **USB** HID |
| Connected to GND | **BLE** HID |

```
XIAO D9 ────o/ o──── GND        (switch closed at boot = BLE)
```

A toggle switch, a jumper or a momentary button all work. In BLE mode, holding D9 LOW
*again after it has been released* keeps the 60-second configuration window open (see
README). A switch left permanently closed does **not** block HID.

The `mode` value stored in the device config is currently informational only. D9 decides
the mode on every boot.

## Shift registers (74HC165), up to 56 buttons

### 74HC165 pinout (DIP-16 / SOIC-16)

```
              ┌───∪───┐
   SH/LD  1 ──┤       ├── 16  VCC  (3V3 only!)
     CLK  2 ──┤       ├── 15  CLK INH  → GND
       E  3 ──┤       ├── 14  D
       F  4 ──┤       ├── 13  C
       G  5 ──┤       ├── 12  B
       H  6 ──┤       ├── 11  A
     /QH  7 ──┤       ├── 10  SER
     GND  8 ──┤       ├──  9  QH
              └───────┘
```

Inputs A–H are sometimes labelled D0–D7 in datasheets. Leave **/QH (pin 7) unconnected**; it's
the *inverted* output and must not be used for chaining.

### Daisy chain

Every chip shares SH/LD (→ D8) and CLK (→ D7). Data flows from the far end of the chain toward
the XIAO: each chip's **QH (pin 9)** goes to the next chip's **SER (pin 10)**, and the QH of the
chip nearest the XIAO goes to **D6**. Tie the SER of the far-end chip to **3V3**. With the default inverted logic, a HIGH reads as
"released", so any chain positions beyond the fitted chips stay released.

```
            far end                                          nearest XIAO
 3V3 → SER[Chip 1]QH → SER[Chip 2]QH → … → SER[Chip 7]QH ──────────► D6
        buttons 1–8    buttons 9–16          buttons 49–56

 D7 ──► CLK   of every chip        D8 ──► SH/LD of every chip
 GND ─► CLK INH and GND of every chip; 3V3 ─► VCC of every chip
```

With fewer chips, the numbering still starts at the far end: with 3 chips, Chip 1 (far end,
SER → 3V3) is buttons 1–8 and the chip on D6 is buttons 17–24. **Set "Number of chips" in the
configurator to the number of chips actually fitted.** A mismatch shifts every button.

### Button inputs

The 74HC165 has no internal pull-ups. Give every input (A–H) a 10 kΩ pull-up to 3V3 and wire
the switch from that input to GND:

```
3V3 ── 10kΩ ──┬── 74HC165 input (A…H)
              └── switch ── GND
```

A released button reads HIGH and a pressed one reads LOW. The firmware default
**`inverted = true`** (configurator: *Invert Logic* checked) turns that into pressed = 1. Tie
any unused input HIGH (through its pull-up) so it reads as released.

### Button numbering

HID button numbers as shown by Windows / `jstest` / gamepad-tester (1-based):

| Chip (1 = far end) | Input A | … | Input H |
|--------------------|---------|---|---------|
| 1 | 1  | … | 8  |
| 2 | 9  | … | 16 |
| 3 | 17 | … | 24 |
| 4 | 25 | … | 32 |
| 5 | 33 | … | 40 |
| 6 | 41 | … | 48 |
| 7 | 49 | … | 56 |

(Firmware/configurator indices are 0-based: HID button 1 = index 0.)

### Direct GPIO buttons (optional)

D4, D5 and D10 can take a button straight to GND (internal pull-up, no resistor). Enable them in
the configurator. **Enabling any GPIO button moves every shift-register button up by 16** (SR
button 1 becomes HID button 17), so with 7 chips the last 8 SR buttons no longer fit in the
64-button report. For 56 SR buttons, leave the GPIO buttons disabled.

## Potentiometers (analog axes)

```
3V3 ──── pot end
A0  ──── pot wiper      (A1, A2, A3 for axes 1–3)
GND ──── pot other end
```

Axes are **disabled by default** because a floating ADC pin produces random axis movement. Enable
only the axes you have wired, then calibrate them in the configurator. A 100 nF capacitor from
each wiper to GND reduces jitter.

## Battery (optional)

Solder a single-cell 3.7 V LiPo to the **BAT+ / BAT−** pads on the underside of the XIAO. The
on-board charger charges it while USB is connected. In BLE mode the firmware reads the battery
voltage every 60 s (via the on-board divider on P0.31, enabled through P0.14) and reports it
through the BLE Battery Service. The voltage-to-percent conversion is a rough linear estimate.

## Safety notes

- 3.3 V only on every pin. A 74HC165 powered from 5 V will put 5 V on D6.
- Observe LiPo polarity. Reversing it destroys the charger.
- Unconnected but enabled inputs cause phantom input. Enable only what's wired.
