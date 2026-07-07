/**
 * SeedJoy - Force Feedback build gate
 *
 * Single switch for the whole FFB stack. Kept dependency-free (no Arduino.h) so
 * the pure engine/runtime compile on the host, and so config.h can include it.
 *
 * ENABLE_FFB == 0 (default): the FFB translation units compile to nothing and
 * the USB path is untouched — the built firmware is byte-identical to the
 * non-FFB baseline. Host unit tests build with -DENABLE_FFB=1.
 *
 * ENABLE_FFB == 1: USB enumerates as a PID force-feedback device (new PID,
 * report IDs, OUT endpoint). This path is NOT hardware-validated yet — see
 * docs/ffb-plan.md. Do not ship it enabled until Phase 2/3 acceptance passes.
 */
#ifndef FFB_CONFIG_H
#define FFB_CONFIG_H

#ifndef ENABLE_FFB
#define ENABLE_FFB 0
#endif

// Distinct USB Product ID for the FFB identity so hosts don't reuse the cached
// non-FFB descriptor. VID stays the same (config.h usbVID).
#ifndef FFB_USB_PID
#define FFB_USB_PID 0x80F5
#endif

#endif // FFB_CONFIG_H
