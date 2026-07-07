# SeedJoy Firmware Audit

**Date:** 2026-07-04
**Scope:** Full firmware (`firmware/seedjoy/`), configurator BLE write path, docs. Suspect library
behaviors were verified against the installed core at
`~/Library/Arduino15/packages/Seeeduino/hardware/nrf52/1.1.10` (Bluefruit52Lib, Adafruit_LittleFS).
**Context:** Audit performed with the project goal of evolving SeedJoy into a **force-feedback
(FFB) capable HID device**. See `docs/ffb-plan.md` for the FFB roadmap.

**Summary:** Solid input-only controller scaffold, but as of commit `e46c91a`:
the tree **does not compile** (B1), **BLE HID never presents the joystick descriptor** (B2), and
**config persistence self-corrupts on the second save** (B3). There is currently **zero FFB
support** — no PID descriptor, no OUT endpoint, no effect engine, no actuator hardware path, and
no free pins.

---

## Fix status (2026-07-07)

Applied in the working tree (not yet committed):

| Item | Status | Notes |
|------|--------|-------|
| B1 | **Fixed** | `ble_config.cpp` float parser now matches `"key":` (numeric). |
| B2 | **Fixed** | BLE HID switched to `BLEHidGeneric(1,0,0)`; `Report ID (1)` added to map; `setReportMap`/`setReportLen`/`setHidInfo` moved before `begin()`; both `inputReport` sites → ID 1. |
| B3 | **Fixed** | `storage.cpp` removes the file before each write (kills append growth). |
| H1 | **Fixed** | Conn interval clamped to ≥6 (7.5 ms), return checked; default 2→6; comment corrected. |
| H3 | **Fixed** | SR buttons now debounced (per-bit, same `DEBOUNCE_MS` policy as GPIO). |
| H4 | **Fixed (calibrate)** | `readVBAT()` drives `VBAT_ENABLE` LOW, sets `AR_INTERNAL_3_0`, restores; percentage now written to `blebas_`. Divider comp = 2.96 for the XIAO 1M/510k divider — **verify against a multimeter** (board-specific, not in BSP). |
| M7 | **Fixed** | README + hardware.md pin drift corrected (D9=P1.14, SR-based button map); stale/corrupted pin sections replaced; fabricated ASCII sketch flagged as non-authoritative. |
| **H2** | **Deferred — needs your decision** | Requiring `SECMODE_ENC_NO_MITM` on the config/calibrate characteristics is a real hardening win, but it forces BLE pairing and **may break the zero-install WebBluetooth configurator** on some platforms. This cannot be verified without hardware + browsers, so it is intentionally NOT applied. Options: (a) require encryption and accept a pairing step; (b) gate config writes behind the existing 60 s physical-button window; (c) accept the risk for a hobby device. Left as-is (`SECMODE_OPEN`) pending your call. |
| M1–M6, M8 | Not addressed | Lower priority; see table below. M5/M8 are folded into the FFB plan (`docs/ffb-plan.md`). |

**Verification:** these are firmware changes that cannot be flashed here. The objective gate is a
clean compile — see the "Compile" note at the bottom of this file for its status.

---

## Blockers

### B1. Stray `\` — tree does not compile
`firmware/seedjoy/ble_config.cpp:249` (`bleExtractJsonFloat`):

```cpp
String s = "\"" + key + \":\"";   // stray '\' in program → hard compile error
```

Verified byte-for-byte in both the working tree and `HEAD`. The `build/` artifacts are stale and
predate this line.

Two bugs in one: even "fixed" to `"\":\""`, the pattern searches for `"key":"` (a *quoted* value)
and never matches numeric JSON such as `"expoFactor":0.25`. The correct pattern is `"\":"` as used
in `seedjoy.ino` (`extractJsonFloat`, ~line 608). Consequence once compiling: `expoFactor` can
never be updated over BLE.

**Fix:** `String s = "\"" + key + "\":";`

### B2. BLE HID enumerates as keyboard+mouse, not joystick
`firmware/seedjoy/ble_hid.cpp:77-81` calls `blehid_.begin()` **before** `setReportMap()` /
`setHidInfo()`. Verified in Bluefruit52Lib source:

- `BLEHidAdafruit::begin()` installs the stock **keyboard/consumer/mouse** report map and creates
  the GATT Report Map + input-report characteristics (report IDs 1/2/3, fixed lengths 8/2/5 bytes)
  inside `begin()`.
- `setReportMap()` after `begin()` only stores a pointer that is never read again — the Report Map
  characteristic already holds the Adafruit default.
- `inputReport(0, &report_, 16)` maps ID 0 → `_chr_inputs[0]` = the **keyboard** characteristic
  (fixed 8 bytes) and notifies it with 16 bytes.

Net effect: over BLE the host sees a keyboard/mouse; joystick data goes nowhere. (Matches the
"ble fix WIP" commit.)

**Fix:** Use `BLEHidGamepad` (ships in the same library), or subclass
`BLEHidGeneric(1 /*input*/, 0, 0)` and call `setReportMap()` + `setReportLen()` **before**
`begin()`. Note the library writes report-reference IDs starting at 1, so add Report ID 1 to the
descriptor and prepend it consistently.

### B3. Config storage corrupts on second save
`firmware/seedjoy/storage.cpp:66` opens with `FILE_O_WRITE` and never removes/truncates the file.
Verified in `Adafruit_LittleFS_File.cpp`: `FILE_O_WRITE` = `LFS_O_RDWR | LFS_O_CREAT` **plus
seek-to-end** — i.e. append mode, no truncate.

First save works. Every later save appends another whole struct; the file grows to 2×, 3×…
`sizeof(DeviceConfig)`. On next boot the size check (`storage.cpp:103`) fails and the device
**silently reverts to defaults**.

**Fix:** `InternalFS.remove(CONFIG_FILENAME);` before opening for write.

---

## High priority

### H1. BLE connection interval out of spec and silently ignored
Default `bleConnInterval = 2` (2.5 ms) is below the BLE minimum of 6 × 1.25 ms = 7.5 ms.
`sd_ble_gap_ppcp_set` rejects it; the unchecked call at `ble_hid.cpp:94` fails, and the device
actually runs with the library's 11.25–15 ms (set inside `BLEHidAdafruit::begin()`).
Also `config.h:150` comments the field as "7.5ms units" while the code treats it as 1.25 ms units.

**Fix:** clamp to ≥6, check the return value, correct the comment/default (6–12 is sensible).

### H2. BLE config service is wide open
All config/monitor/calibrate characteristics use `SECMODE_OPEN`
(`ble_config.cpp:44-87`). Anyone in radio range can silently rewrite the device config (pins,
name, SR layout) or fire calibration commands — no pairing required.

Related: the comment at `ble_hid.cpp:53-55` claiming security/bonding is "disabled" is wrong —
`setIOCaps(false,false,false)` merely selects Just-Works pairing; the HID characteristics still
require encryption per library defaults.

**Fix:** require encryption (`SECMODE_ENC_NO_MITM`) on write characteristics, and/or gate config
writes behind a physical-button window (the 60 s connect window already exists — reuse it).

### H3. No debounce on shift-register buttons
GPIO buttons are debounced (`buttons.cpp:90-100`); the 56 SR buttons are raw reads shipped at up
to 1 kHz over USB. Mechanical chatter will double-fire in games.

**Fix:** apply the same time-based debounce after `shiftRegisters_.update()` (per-bit last-change
timestamps, or a 2-of-3 vote across reads).

### H4. Battery monitoring is a stub and the math is wrong for XIAO
- `updateBatteryLevel()` (`seedjoy.ino:357-382`) computes a percentage but **never writes
  `blebas_`** — the README's "LiPo voltage → BLE battery service" is not implemented.
- `readVBAT()` never drives `VBAT_ENABLE` (P0.14) LOW, so the XIAO's divider is disconnected and
  the ADC pin floats.
- Compensation factor 2.0 is the Adafruit Feather (100k/100k) value; the XIAO's 1 MΩ/510 kΩ
  divider needs ≈ ×2.96.
- `VBAT_DIVIDER` is defined but unused.

---

## Medium / minor

| # | Issue | Location |
|---|-------|----------|
| M1 | Mixed-mode button overflow: with any GPIO button enabled, SR offset = 16, so with 7 chips SR buttons map to logical 16–71 and the last 8 silently fall off the 64-bit HID report. | `buttons.cpp:34-48` |
| M2 | Single-char command shortcut consumes any line starting with `C/c/H/h` — fragile if a future command starts with those letters. | `seedjoy.ino:479-489` |
| M3 | Brace-depth framing counts `{`/`}` inside JSON strings — a device name containing a brace breaks config-write reassembly. | `ble_config.cpp:configWriteCallback` |
| M4 | `usbPollRate` only throttles the main loop; the endpoint bInterval is hard-coded to 1 ms in the `Adafruit_USBD_HID` constructor. Conflated concepts. | `usb_hid.cpp:41`, `seedjoy.ino:186` |
| M5 | USB `hidReportCallback` is an empty stub and the GET_REPORT callback is `nullptr` — fine today, but this is the exact plumbing FFB needs (see plan). | `usb_hid.cpp:103-108` |
| M6 | Hand-rolled JSON extraction searches the whole payload per key; nested-scope collisions are avoided today only by careful key naming. Escaped quotes in `deviceName` break parsing. | `.ino` + `ble_config.cpp` |
| M7 | Doc drift: README says D9 = P0.12; `config.h:30` says P1.14 (config.h is correct for XIAO). README's "up to 64 total buttons" contradicts M1. | README, `config.h` |
| M8 | Blocking `Serial` JSON printing in the main loop (button stream, config dump) adds jitter to the 1 kHz report loop — matters much more once an FFB force loop exists. | `seedjoy.ino:214-225` |

---

## What's in good shape

- Clean module separation (axes / buttons / SR / storage / transport); easy to extend.
- Axis pipeline (calibration → deadzone → curve → smoothing) is correct and readable; 12-bit ADC
  configured properly in `axes.begin()`.
- SR read timing is conservative and correct for 74HC165 (≈112 µs for 7 chips — no loop-budget risk).
- Config CRC32 + magic + version validation is sound (once B3 is fixed).
- Ghost-input protection (60 s config window, SR re-init before enabling HID) is thoughtful.
- USB-mode HID input path works as designed (descriptor without report IDs + `sendReport(0, …)` is
  correct TinyUSB usage).

---

## Compile

After the fixes above, the tree builds clean:

```
arduino-cli compile --fqbn Seeeduino:nrf52:xiaonRF52840 .
→ exit 0; Sketch 158064 bytes (19%) flash, 20300 bytes (8%) RAM
```

This closes the B1 blocker (which was a hard compile error) and confirms the B2 BLE type-swap
introduced no build breakage. It does **not** substitute for on-hardware testing: the BLE
enumeration (B2), config persistence across reboots (B3), SR debounce feel (H3), and battery
voltage accuracy (H4 divider constant) still need a flashed device to validate.
