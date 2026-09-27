# SeedJoy Test Board rev A: parts list (India-sourced)

All parts are through-hole and commonly stocked by Indian hobby-electronics stores. Quantities
are for one board. The links are **store searches** (Indian store pages block automated checks,
so they aren't verified listings). Pick any in-stock item that matches the description.
Fallback for anything out of stock: DigiKey India (digikey.in) or Mouser India (mouser.in),
which ship to India.

Store search shortcuts:
Robu `https://robu.in/?s=<query>&post_type=product` ·
Robocraze `https://robocraze.com/search?q=<query>` ·
Evelta `https://evelta.com/search.php?search_query=<query>`

| Ref | Qty | Part (what to search for) | Notes | Search |
|-----|-----|---------------------------|-------|--------|
| (plugs into J1/J2) | 1 | **Seeed Studio XIAO nRF52840** (non-Sense), SKU 102010448 | Sold by Robu, Robocraze, Evelta. Not the "Sense" or "Plus" variants. Solder its 2×7 male headers if it comes without | [Robu](https://robu.in/?s=XIAO+nRF52840&post_type=product) · [Robocraze](https://robocraze.com/search?q=XIAO+nRF52840) · [Evelta](https://evelta.com/search.php?search_query=XIAO+nRF52840) · [Seeed](https://www.seeedstudio.com/Seeed-XIAO-BLE-nRF52840-p-5201.html) |
| J1, J2 | 2 | 1×7 female header, 2.54 mm | Or cut from a 1×40 female strip | [Robu](https://robu.in/?s=female+header+2.54&post_type=product) |
| U1 | 1 | **74HC165** shift register, DIP-16 | Must be 74**HC**165 (not LS) | [Robu](https://robu.in/?s=74HC165&post_type=product) · [Evelta](https://evelta.com/search.php?search_query=74HC165) |
| U1 socket | 1 | 16-pin DIP IC socket (300 mil) | | [Robu](https://robu.in/?s=16+pin+ic+socket&post_type=product) |
| C1 | 1 | 100 nF ceramic capacitor ("104"), 5 mm lead spacing | | [Robu](https://robu.in/?s=104+ceramic+capacitor&post_type=product) |
| RN1 | 1 | **10 kΩ resistor network, 9-pin SIP, bussed** ("A103J" / "9 pin 10K") | Pin 1 (dot) is the common pin and goes to the square pad | [Robu](https://robu.in/?s=10k+resistor+network+9+pin&post_type=product) · [Evelta](https://evelta.com/search.php?search_query=resistor+network+10k) |
| R1 | 1 | 10 kΩ ¼ W resistor | Chain-input pull-up | [Robu](https://robu.in/?s=10k+resistor+1%2F4w&post_type=product) |
| SW1–SW8 | 8 | 6×6 mm tactile push button, 4-pin THT | Any height | [Robu](https://robu.in/?s=6x6+tactile+switch&post_type=product) |
| SW9 | 1 | 4-position DIP switch, 2.54 mm | | [Robu](https://robu.in/?s=4+way+dip+switch&post_type=product) |
| U2 | 1 | **PS2 2-axis thumbstick (3D rocker potentiometer with push)**. Easiest: buy a **KY-023 / PS2 joystick module** (₹75–150) and desolder its stick | Footprint = Alps RKJXV1224005 pattern. **Before ordering the PCB, check with calipers that the pot pins are 2.5 mm apart** | [Robocraze](https://robocraze.com/products/joystick-module) · [Robomart](https://robomart.com/product/ps2-joystick-module-breakout-sensor/) · [Robu](https://robu.in/?s=ps2+joystick&post_type=product) |
| J3 | 1 | 1×5 female header, 2.54 mm | External analog input: GND, 3V3, X, Y, SW | [Robu](https://robu.in/?s=female+header+2.54&post_type=product) |
| J4–J9 | 1 strip | 2×40 **male** pin header, 2.54 mm. Cut 2× 2×4 (J4, J5), 1×2 (J6), 1×5 (J7), 2× 1×3 (J8, J9) | | [Robu](https://robu.in/?s=2x40+male+header&post_type=product) |
| (on J4, J5, J6, J8, J9) | 5 | 2.54 mm jumper caps (shunts) | Buy a pack | [Robu](https://robu.in/?s=jumper+cap+2.54&post_type=product) |
| H1–H4 | 4 | M3 nylon standoff + screw set | | [Robu](https://robu.in/?s=m3+standoff&post_type=product) |

## Order plan (for someone who already has the XIAO, headers, 74HC165 + socket, R and C)

**Order 1: Sharvi Electronics (covers everything left)**

| Ref | Buy | Qty (incl. spares) | Link |
|-----|-----|-----|------|
| RN1 | 10k Ohm 9-pin resistor network (SIP) | 2 | [sharvielectronics.com](https://sharvielectronics.com/product/10k-ohm-9-pin-resistor-network-sip/) |
| SW1–SW8 | 6x6x5 mm tactile push button, DIP | 10 | [sharvielectronics.com](https://sharvielectronics.com/product/6x6x5mm-tactile-push-button-switch-black-dip-package-2/) |
| SW9 | DIP switch, 4 way | 1 | [sharvielectronics.com](https://sharvielectronics.com/product/dip-switch-4-way/) |
| U2 | Dual Axis XY Joystick Module (desolder the stick) | 2 | [sharvielectronics.com](https://sharvielectronics.com/product/dual-axis-xy-joystick-module/) |
| jumpers | Jumper cap 2.54 mm | 6 | [sharvielectronics.com](https://sharvielectronics.com/product/jumper-cap-2-54-mm-pitch-black/) |
| H1–H4 | M3 x 10 mm male-female nylon hex spacer (+ 4 M3 nuts) | 4 | [sharvielectronics.com](https://sharvielectronics.com/product/m3x10mm-male-to-female-nylon-hex-spacer/) |

Buy 2 joystick modules: one to desolder for the paper-fit test (pcb/README.md), one spare.
Don't substitute their "Thumb Joystick Switch XY Dual Axis 4 Way Direction" listing without
checking it: its page gives no specs, and "4-way" may be a digital navigation switch.

**Order 2 (backup if Sharvi is out of stock)**

| Part | Alternatives |
|------|--------------|
| 10k 9-pin network | [RoboComp](https://robocomp.in/product/10k-ohm-9-pin-resistor-network-sip/), [Kitsguru](https://kitsguru.com/products/a103j-network-resistance-9-pin-10k) |
| 4-way DIP switch | [Robu (pack of 3)](https://robu.in/product/4-way-slide-switch-2-54mm-pitch-pack-of-3/), [REES52](https://rees52.com/products/4-position-dip-switch-rs224) |
| Joystick module | [Robu](https://robu.in/product/joystick-module-ps2-breakout-sensor/), [REES52](https://rees52.com/products/ps2-joystick-module-dual-axis-xy-analog-joystick-breakout), [Robocraze](https://robocraze.com/products/joystick-module) |
| M3 standoffs | [REES52 320-pc M3 nylon kit](https://rees52.com/products/320pcs-m3-male-female-spacer-standoff-kit-320pcs-m3-male-female-nylon-hex-spacer-standoff-screw-nut-assorted-assortment-kit-black-rs1806) |

**PCB:** upload `seedjoy_testboard_gerbers.zip` to JLCPCB or PCBWay (both ship to India).

## Jumper settings

| Jumper | Setting | Effect |
|--------|---------|--------|
| J8 X_SRC | 1-2 | X comes from the on-board stick |
|          | 2-3 | X comes from the J3 header (external sensor) |
| J9 Y_SRC | 1-2 / 2-3 | Same, for Y |
| J4 X_SEL | row n | The selected X source goes to An (A0…A3) |
| J5 Y_SEL | row n | The selected Y source goes to An. **Never the same row as J4** |
| J6 | on | Stick push-button (on-board or J3 SW) acts as BTN8 |

Then enable the matching axes in the configurator.

## Notes

- **Chaining your own 74HC165 board (J7):** J7 carries 3V3, GND, CLK (D7), LOAD (D8) and SER-in.
  Wire your chain's last QH to J7 pin 5 (SER). Your chips become buttons 1…8N and this board's
  8 buttons follow. Set *Number of chips* = N + 1. With nothing connected, R1 holds SER high.
- **Everything runs at 3.3 V.** The stick's pots and anything on J3 are powered from 3V3, which
  matches the XIAO's ADC range. Never put 5 V on any pin.
- **Stick push switch** is wired between diagonal legs a and d, so it works whichever way the clone
  pairs its 4 legs internally.
- **Solder order:** IC socket, resistor network, R1, C1, DIP switch, tactile switches, headers,
  thumbstick last. Insert the 74HC165 with its notch matching the silkscreen.
- **PCB:** 2 layers, 1.6 mm FR-4. JLCPCB and PCBWay both ship to India. Upload the Gerber zip.
