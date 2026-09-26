# SeedJoy FFB — Physical Test Plan

These are the tests **only you can run**, because they need real USB enumeration, a host OS, and
(for some) a motor. Everything checkable in software is already covered by 67 host-side unit
assertions (`firmware/tests/run_tests.sh` — effect math, the full PID report/handshake path, and a
descriptor structural validator). What remains is the one thing no unit test can prove: **that a
real host accepts the PID descriptor and drives the device**, plus the on-hardware timing/safety and
(optionally) motor behavior.

Work top to bottom — each test gates the next. **Tests 1–7 need NO motor** (force is observed on the
on-board status LED, which mirrors axis-0 |force|). Motor tests are in §8.

Legend: 🔌 = no motor needed · ⚙️ = needs a motor wired · 📈 = better with an oscilloscope/logic analyzer.

---

## 0. Build & flash the FFB firmware

```bash
cd firmware/seedjoy
# Build with FFB enabled (default build has it OFF and is the normal input device)
~/bin/arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 \
  --build-property "compiler.cpp.extra_flags=-DENABLE_FFB=1" --output-dir build_ffb .
# Flash: make a UF2 and copy it to the bootloader drive (double-tap RESET on the XIAO first):
#   python3 ~/Library/Arduino15/packages/Seeeduino/hardware/nrf52/*/tools/uf2conv/uf2conv.py \
#     -f 0xADA52840 -c -o build_ffb/seedjoy.uf2 build_ffb/seedjoy.ino.hex
# or:
~/bin/arduino-cli upload -p /dev/cu.usbmodem* --fqbn Seeeduino:nrf52:xiaonRF52840 --input-dir build_ffb .
```

- The USB device now identifies as **VID `0x239A`, PID `0x80F5`** (the FFB identity; the non-FFB
  build is `0x80F4`).
- Force select **USB mode**: hold the D9 mode pin HIGH at boot (or connect over USB — see the main
  README). FFB runs in USB mode only.
- **Enable + calibrate at least axis 0 before the force tests.** Axes ship **disabled** (an audit
  safety default), and a disabled axis reports position 0 — so **position/velocity effects (spring,
  damper) produce zero force and a dark LED no matter how you move the pot** until the axis is
  enabled. Do this in the web configurator (enable axis 0, run its min/center/max calibration).
  Constant/sine/ramp don't depend on position, so they'll "work" even with axes disabled — don't let
  that fool you into thinking the spring is broken.
- **Actuator enable:** force output also requires the host to send PID Device Control "enable
  actuators". Linux `hid-pidff` does this automatically when an effect is played (so it's not a
  manual step via `fftest`); only matters if you drive the device from a raw script.

**Tools you'll need** (Linux is the easiest host for FFB bring-up):
- `linuxconsoletools` (provides `fftest`, `jstest`, `evtest`) — `apt install linuxconsoletools`
- `usbutils` (`lsusb`, `usbhid-dump`) and optionally `hidrd` (`hidrd-convert`) for descriptor decode
- `usbmon` + Wireshark (or `tshark`) for the control-transfer handshake
- Windows (optional, later): `joy.cpl`, and `FEdit`/`fedit` from the legacy DirectX SDK

---

## 1. 🔌 Descriptor decodes cleanly

**Purpose:** the descriptor is the single unvalidated artifact. Confirm a real HID parser accepts it
before anything else.

```bash
# Dump the raw HID report descriptor the device advertised:
sudo usbhid-dump -d 239a:80f5 | grep -v : | xxd -r -p > /tmp/seedjoy.rdesc
# Decode it (any of):
hidrd-convert -i natv -o spec /tmp/seedjoy.rdesc      # human-readable
#   ...or paste the bytes into https://eleccelerator.com/usbdescreqparser/
```

**Expected:** decoder reports **no errors**; you can see the Joystick collection (Report ID 1, 4×
16-bit axes + 64 buttons) and a PID collection with report IDs 2–15. Report byte sizes match those
in `ffb_reports.h` (the unit test `ffb_descriptor_test` already checks this against the structs).

- [ ] **PASS / FAIL:** decoder accepts descriptor, no errors → __________

> If FAIL: note the exact item the decoder rejects. This is expected to be the riskiest step; the
> fix is usually a wrong PID *usage* value in `ffb_reports.cpp` (sizes are already test-locked).

## 2. 🔌 Joystick regression (the input device must still work)

**Purpose:** adding the PID collection + Report ID 1 must not break the joystick.

```bash
jstest --normal /dev/input/js0        # or by-id path from: ls /dev/input/by-id/
# move each pot, press buttons
```

**Expected:** 4 axes track the pots; up to 56 buttons register. Windows: `joy.cpl` → Properties
shows the same.

- [ ] **PASS / FAIL:** axes + buttons work with Report ID 1 → __________

## 3. 🔌 PID enumeration + Create→Block Load→Pool handshake

**Purpose:** confirm the OUT endpoint and feature-report handshake work on real USB — the part the
unit tests exercise only through synthetic bytes.

```bash
# Is it recognized as a force-feedback device?
udevadm info -q property /dev/input/event* | grep -i FF        # or:
cat /proc/bus/input/devices                                    # look for "FF" in Handlers/features
# Capture the handshake:
sudo modprobe usbmon
sudo wireshark -k -i usbmon1     # then re-plug the device / start fftest in test 4
```

**Expected:** the device exposes force-feedback capability; in usbmon you see, on effect creation, a
**Create New Effect** feature SET (report 13) followed by a **Block Load** feature GET (report 14)
returning status = 1 (success) and a valid block index, and a **Pool** GET (report 15).

- [ ] **PASS / FAIL:** FF capability present; Block Load returns success → __________

## 4. 🔌 Every effect uploads

**Purpose:** each `Set*` report is parsed and each effect type allocates a block.

```bash
fftest /dev/input/by-id/usb-*SeedJoy*-event-joystick
# In the menu, upload/play each: constant, spring, damper, sine (periodic), ramp, ...
```

**Expected:** every effect uploads without "upload failed"; the pool holds up to 16 concurrent
effects (`MAX_FFB_EFFECTS`) and reports full beyond that.

- [ ] **PASS / FAIL:** all effect types upload → __________

## 5. 🔌📈 Force output is correct (observed on the LED / scope)

The status LED brightness = axis-0 **|force|** (magnitude only; sign needs a motor or a scope on the
DIR pin once wired). Run each from `fftest`.

> **Prerequisite (spring/damper only):** axis 0 must be enabled + calibrated (see §0). A disabled
> axis reports position 0, so spring/damper output is always zero — you'd see a dark LED and wrongly
> conclude the effect is broken.

| Effect | Do this | Expected on the LED |
|--------|---------|---------------------|
| **Constant** | play a constant force, vary its level | LED brightness tracks the level |
| **Spring** | play a spring, move axis-0 pot off center | LED brightens as displacement grows; **dark at center** |
| **Damper** | play a damper, move the pot fast vs. slow | LED brightens with pot **speed**, dark when still |
| **Sine (periodic)** | play a 2–5 Hz sine | LED **pulses** at that frequency |
| **Ramp** | play a ramp over ~2 s | LED brightness ramps over the duration |

📈 With a scope on a motor PWM pin (after §8) you also confirm **direction/sign**: spring output
opposes displacement; sine alternates direction.

- [ ] **PASS / FAIL:** constant __ · spring centers __ · damper __ · sine freq __ · ramp __

## 6. 🔌 Safety behaviors

**Purpose:** the device must never hold force when it shouldn't.

- **USB detach:** with a constant force playing, unplug USB. → LED goes **dark within ~10 ms**
  (the tick calls `disableAll()` when `TinyUSBDevice.mounted()` is false).
- **PID Device Control:** from `fftest`, stop-all / pause → LED dark; continue → force resumes.
- **Device Gain:** set gain to 50% → LED level roughly halves for the same effect.
- **Global duty clamp:** (optional) rebuild with `-DFFB_MAX_DUTY=0.5f`; max LED brightness caps at
  ~half even at full effect + full gain.

- [ ] **PASS / FAIL:** detach kills output __ · pause/stop __ · gain scales __ · duty clamp __

## 7. 🔌 Concurrency stress (validates the mutex fix)

**Purpose:** the one property that can't be unit-tested — the `usbd`-task callbacks vs. the `loop`
tick are serialized by `ffb_lock` and never hang or glitch under load.

```bash
# Hammer effect parameter updates while a force loop runs. Option A: a loop of fftest uploads.
# Option B (better), a tiny SDL2 haptic program updating a constant effect's level every ~2 ms
# for a few minutes while you watch the LED and dmesg.
dmesg -w    # watch for USB errors/timeouts in another terminal
```

**Expected:** runs for minutes with **no USB stalls, no device reset, no LED freeze/glitch**; the
force keeps responding. A hang or `dmesg` USB timeout would indicate a lock/ordering bug.

- [ ] **PASS / FAIL:** sustained upload load, no stall/glitch → __________

---

## 8. ⚙️ Motor bring-up (Phase 4)

Only after §1–7 pass. **No motor pins are assigned by default** — you must choose a driver and edit
`firmware/seedjoy/ffb_motor.cpp::begin()`. Recommended first rig: one axis, DRV8871.

```cpp
// in MotorOutput::begin()
axis_[0].type   = MOTOR_DRIVER_DUAL_PWM;  // DRV8871 IN1/IN2
axis_[0].pinA   = D4;                      // IN1
axis_[0].pinB   = D5;                      // IN2
axis_[0].invert = false;                   // set true if the spring pushes AWAY from center
```

**Electrical (do not skip):** separate motor supply sized for stall current, **common ground** with
the XIAO, flyback/TVS protection. Start with `-DFFB_MAX_DUTY=0.3f` and a small motor.

**Safety first:** confirm §6 detach test **with the motor wired** — unplug USB, motor must go limp
immediately.

Then re-run §5 on the motor:
- [ ] ⚙️ **Constant** — motor pushes one way, reverses with sign
- [ ] ⚙️ **Spring** — release off-center, motor **centers** the axis (if it pushes away, flip `invert`)
- [ ] ⚙️ **Damper** — axis feels heavier when moved fast
- [ ] ⚙️ **Sine** — motor oscillates at the effect frequency
- [ ] ⚙️ Direction/sign matches the game (tune `invert` / the polar-direction mapping)

**Windows validation (optional, after Linux is clean):** `joy.cpl` shows the device; `FEdit` (legacy
DirectX SDK) creates/plays effects. Windows' PID driver is stricter than Linux — if Linux passes but
Windows rejects enumeration, the descriptor's Block Load status codes / Pool report are the usual
culprits.

---

## Results summary (fill in)

| # | Test | Motor? | Result | Notes |
|---|------|--------|--------|-------|
| 1 | Descriptor decodes | no | | |
| 2 | Joystick regression | no | | |
| 3 | PID enum + handshake | no | | |
| 4 | Effects upload | no | | |
| 5 | Force correctness (LED) | no | | |
| 6 | Safety | no | | |
| 7 | Concurrency stress | no | | |
| 8 | Motor bring-up | yes | | |

Send this back with the FAIL rows filled in (and the decoder output / usbmon capture for any failure)
and I can turn each into a specific firmware fix.
