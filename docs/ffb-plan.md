# SeedJoy Force-Feedback (FFB) Plan

**Goal:** evolve SeedJoy into a force-feedback HID device that games recognize natively
(DirectInput / SDL haptics), rendering effects on real actuators.

**Ground rules established by the audit (`docs/audit.md`):**

- FFB will be implemented as a **USB HID PID** (Physical Interface Device, usage page 0x0F)
  device. That is the only path DirectInput/SDL treat as force feedback.
- **BLE stays input-only.** No mainstream host OS delivers PID over BLE HID (HOGP); commercial
  controllers do wireless rumble only via proprietary reports + custom drivers. Additionally, FFB
  motors need a mains/bench supply — battery operation and FFB are mutually exclusive in practice.
- Nothing off-the-shelf exists for nRF52840 + TinyUSB — the PID descriptor, report plumbing, and
  effect engine are custom work. The MCU itself is comfortably capable (1 kHz HID, 4× PWM
  peripherals, 256 KB RAM).

---

## Implementation status (2026-07-08)

Phases 1–3 are implemented to **production-grade code** plus a working Phase 4 motor abstraction,
gated behind `ENABLE_FFB` (default **0**). **With the gate off, the built firmware is byte-identical
to the non-FFB baseline** — re-verified after all production changes by diffing the compiled flash
image against commit `8cd31f9`. Turn it on with `-DENABLE_FFB=1` (or edit `ffb_config.h`).

The one thing that *cannot* be verified without hardware is the USB PID descriptor's acceptance by a
real host; everything host-verifiable is covered by 67 unit assertions (`firmware/tests/run_tests.sh`).

| Piece | Files | Status | How verified |
|-------|-------|--------|--------------|
| Effect engine — constant, ramp, spring, damper, inertia, friction, periodic, envelope, **per-axis conditions**, **loop count**, mixing, device gain, **hard safety clamp** | `ffb_engine.*`, `ffb_types.h` | **Production, unit-tested** | `ffb_engine_test.cpp` — 31 assertions |
| PID runtime — Create→Block Load→Pool handshake, all `Set*` parsers, **per-axis conditions**, **polar direction**, Device Control/Gain, Effect Operation, Block Free, PID State (playing index) | `ffb_reports.h`, `ffb_runtime.*` | **Production, unit-tested** | `ffb_runtime_test.cpp` — 19 assertions (raw wire bytes) |
| PID report **descriptor** | `ffb_reports.cpp` | **Structurally validated, host-acceptance UNVERIFIED** | `ffb_descriptor_test.cpp` — 17 assertions: collections balance, every report's size matches its struct. Still **not** proven against a real HID stack. |
| Concurrency — engine access serialized across the `usbd` and `loop` tasks | `ffb_lock.*`, `usb_hid.cpp`, `seedjoy.ino` | **Fixed (FreeRTOS mutex)** | compiles; needs the on-hardware stress test (physical plan §7) |
| Phase 1 USB restructure — Report ID 1 joystick, OUT endpoint, GET/SET_REPORT dispatch, bumped PID | `usb_hid.*` | **Implemented, compiles** | both gate states build; **not** host-tested |
| Phase 3 driver — ~1 kHz tick, mutex-guarded engine step, USB-detach safety cutoff | `seedjoy.ino` | **Implemented, compiles** | loop-based (a hardware-timer ISR is a future refinement, not required) |
| Phase 4 motor output — driver-type-aware (PWM+DIR / dual-PWM), global duty clamp, LED viz | `ffb_motor.*` | **Abstraction ready, NO pins assigned** | no actuator wired; axis-0 force mirrored to status LED |

**Concurrency fix (resolved).** USB report callbacks run in the high-priority FreeRTOS `usbd` task
(verified: `tud_task()` loops there — `Adafruit_TinyUSB_nrf.cpp`); the force tick runs in the
low-priority `loop` task. Both touch engine state, so a single mutex (`ffb_lock`) serializes them.
A mutex — not the report queue floated earlier — is correct because the Create→Block Load handshake
is a synchronous control transfer that must be serviced in the usbd task; a queue would defer Create
and make Block Load read stale state. The loop holds the lock only across the pure-math engine step,
so priority inversion is bounded and deadlock impossible.

**The one remaining unknown is the descriptor's host acceptance** — that is what the physical test
plan exists to close. Do **not** treat "compiles + unit tests pass" as "a host drives it."

**Immediate next steps (no motor needed — see `docs/ffb-physical-tests.md`):**
1. Parse `FFB_HID_REPORT_DESCRIPTOR` with a HID decoder; enumerate on Linux, run `fftest`; watch the
   Create→Block Load handshake in `usbmon`.
2. Confirm the joystick half still enumerates (`jstest`) with the new Report ID 1.
3. Stress the concurrency fix (hammer effect uploads while the force loop runs).
4. Then Windows (`fedit`) and, separately, wire a motor (assign pins in `ffb_motor.cpp::begin`).

---

## Phase 0 — Stabilize the baseline (prerequisite)

Fix the audit blockers so FFB work builds on a device that actually works:

1. **B1** compile error in `ble_config.cpp:249` (+ use the correct `"key":` pattern for floats).
2. **B3** LittleFS append bug — `InternalFS.remove()` before save.
3. **B2** switch BLE to `BLEHidGamepad` / `BLEHidGeneric` subclass with the map set before `begin()`.
4. **H3** debounce SR buttons (they'll be trigger buttons for FFB effects).
5. **M8** remove blocking `Serial` prints from the steady-state loop (the force loop cannot
   tolerate multi-ms jitter).

**Acceptance:** clean compile via `compile.sh`; config survives ≥3 save/reboot cycles; BLE mode
enumerates as a joystick on Windows/Android; USB mode unchanged.

## Phase 1 — USB HID restructuring (report IDs + OUT endpoint)

PID forces layout changes on the existing input path:

1. Add **Report ID 1** to the joystick input report (PID needs many IDs; ID-less reports can't
   coexist). Host-visible change — note in README that games may need recalibration.
2. Construct `Adafruit_USBD_HID` with `has_out_endpoint = true`; raise
   `CFG_TUD_HID_EP_BUFSIZE` if needed (64 is fine for PID).
3. Implement real `GET_REPORT` / `SET_REPORT` callbacks (feature + output), replacing the
   `nullptr` / empty stub in `usb_hid.cpp`. Route by report ID to a dispatch table.
4. Keep BLE completely untouched by this phase (separate descriptor already, per Phase 0.3).

**Acceptance:** device still works as a plain joystick in `joy.cpl` / `jstest`; a host-side test
script can round-trip a vendor feature report.

## Phase 2 — PID descriptor + block-management handshake (no motors yet)

Implement the PID class surface so hosts *believe* the device does FFB before any hardware exists.

**Input reports**
- ID 1: joystick (from Phase 1)
- ID 2: **PID State** (paused, actuators enabled, safety switch, playing, effect block index)

**Output reports** (host → device, interrupt OUT):
- Set Effect (block index, type, duration, gain, trigger, axes/direction)
- Set Envelope, Set Condition (×2 axes), Set Periodic, Set Constant Force, Set Ramp Force
- Effect Operation (start / start solo / stop, loop count)
- PID Block Free, PID Device Control (enable/disable actuators, stop all, reset, pause, continue)
- Device Gain

**Feature reports** (control transfers):
- Create New Effect (effect type in) → **PID Block Load** (block index + status out)
- **PID Pool** (pool size, max simultaneous effects, device-managed pool = 1)

**Device side:** static effect pool (start with 16 blocks × largest-effect union — device-managed,
no host pool math), allocation state machine for Create New Effect → Block Load, and a parameter
store the engine will later read. Reference implementations to crib structure (not code) from:
Arduino-FFB-wheel (AVR), VNWheel, OpenFFBoard (STM32).

**Acceptance:**
- Linux: `fftest /dev/input/eventX` lists and uploads all effect types without error.
- SDL: `testhaptic` reports the expected capability mask.
- Windows: enumerates with the PID class driver; `fedit` (DirectX SDK) can create/start effects.
- Wireshark/usbmon shows correct Block Load handshakes. No motor output — state only.

## Phase 3 — Effect engine (1 kHz force loop)

1. **Scheduler:** fixed 1 kHz tick (hardware timer, not `millis()` polling). Input scan + HID send
   stay at their current rates; the force loop must never block on Serial/BLE/flash.
2. **Per-effect rendering:**
   - Constant: magnitude × envelope × gain
   - Ramp: linear start→end over duration
   - Periodic: sine/square/triangle/sawtooth via phase accumulator
   - Condition (spring/damper/inertia/friction): f(position, velocity) with CP offset, dead band,
     ± coefficients, saturation. Position from the existing 12-bit pot pipeline; velocity via
     filtered derivative (e.g. 3-tap backward difference + IIR).
3. **Mixing:** sum active effects → clamp → device gain → signed output per axis.
4. **Safety (non-negotiable):** watchdog on the force loop; zero output on USB suspend/detach,
   PID Device Control "stop all"/"pause", or effect timeout; global duty clamp configurable in
   `DeviceConfig`.

Start with constant + spring + damper (covers most sims), then periodic, then envelopes.

**Acceptance:** with output visualized (logic analyzer / LED PWM), `fftest` spring effect produces
position-proportional opposing output; sine effect produces the right frequency; detach kills
output within 10 ms.

## Phase 4 — Actuator hardware

Pin reality on XIAO (11 GPIOs, all currently allocated): axes D0–D3, GPIO buttons D4/D5/D10
(disabled by default in SR-only mode), SR D6–D8, mode select D9.

| Option | Pins | Fits |
|--------|------|------|
| **A. Single axis, direct drive** — DRV8871 (IN1/IN2) on D4/D5, optional fault/enable on D10 | 2–3 repurposed | 1-axis FFB (joystick X or wheel), simplest |
| **B. Multi-axis via I2C** — D4/D5 are the native I2C pins → PCA9685 → discrete H-bridges (or I2C motor drivers) | 2 pins, expandable | 2+ axes; PCA9685's ~1.5 kHz PWM is audible — prefer real H-bridge drivers with the PCA9685 only as DIR/EN, or pick option A per axis |
| **C. Reclaim D9** — move mode select to a boot-time button combo (hold SR button 0) | +1 pin | Frees a pin for enable/fault sensing |

Recommendation: **Option A first** (one axis end-to-end proves the whole stack), design the engine
for N axes from day one. Drive PWM at 20–25 kHz (inaudible; nRF52840 PWM handles it on any pin).

Electrical requirements: separate motor supply sized for stall current, common ground with the
XIAO, flyback/TVS protection, and the config default `usbPID` should change (new product identity
so hosts don't cache the old non-FFB descriptor — bump `usbPID`, keep VID).

**Acceptance:** spring-centering demo on real hardware; a sim title (e.g. anything SDL2-haptic or
DirectInput-based) produces believable forces.

## Phase 5 — Integration & polish

- Expose FFB settings in `DeviceConfig` + configurator: global gain, max duty, per-effect-type
  enable, motor polarity/axis mapping (extends the existing JSON config path).
- PID State input report kept truthful (paused/enabled/playing) — some games poll it.
- Docs: wiring guide for the driver board, safety notes, supported-effects matrix.
- Regression: BLE input-only mode must remain fully functional and completely FFB-free.

---

## Risks / open questions

1. **TinyUSB feature-report handling** on nRF52 for multi-ID GET_REPORT is the least-tested corner
   — validate early in Phase 2 with a USB analyzer or usbmon before building the engine.
2. **Windows PID driver quirks:** it is strict about Block Load status codes and pool reports;
   most DIY-FFB failures are handshake bugs, not engine bugs. Budget debugging time here.
3. **Pot-based position sensing** limits fidelity (noise → damper jitter). Fine for entry-level;
   a magnetic encoder (I2C, fits option B bus) is the upgrade path for a wheel.
4. **Actuator choice drives everything mechanical** (gearing, torque, back-drivability). The plan
   above is actuator-agnostic through Phase 3, which is deliberate — hardware can be selected in
   parallel.
5. **Engine thread-safety — RESOLVED (mutex).** USB report callbacks mutate engine state from the
   high-priority `usbd` task; the force tick reads it from `loop`. Serialized with a single
   priority-inheriting FreeRTOS mutex (`ffb_lock`); the loop holds it only across the pure-math
   engine step. Correct by construction, but confirm on hardware with the concurrency stress test
   (physical plan §7) — that's the one thing a host test can't exercise.

## Suggested order of work

Phase 0 is a day-scale fix batch and worth doing immediately regardless of FFB. Phases 1–2 are
pure firmware and fully testable without any new hardware. Phase 3 is testable with an LED/scope.
Hardware spend only becomes necessary at Phase 4.
