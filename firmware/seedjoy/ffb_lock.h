/**
 * SeedJoy - Force Feedback concurrency lock
 *
 * A single FreeRTOS mutex serializing ALL FfbRuntime/engine access. On this
 * core, USB report callbacks run in the high-priority "usbd" task (tud_task)
 * while the force tick runs in the low-priority "loop" task; both touch engine
 * state, so they must be serialized. A mutex (not a queue) is used because the
 * Create New Effect -> Block Load handshake is a synchronous control transfer
 * that must be serviced in the usbd task; a queue would defer Create and make
 * Block Load read stale state. Priority inheritance bounds the inversion.
 *
 * INVARIANTS (the whole correctness argument — cannot be unit-tested here):
 *   - Call ffbLockInit() BEFORE usbHID.begin(), i.e. before the usbd task can
 *     fire a callback. ffbLock()/ffbUnlock() are safe no-ops until then.
 *   - The loop must take the lock, run ONLY pure math, and release — never hold
 *     it across a blocking or USB call — so deadlock is impossible.
 */
#ifndef FFB_LOCK_H
#define FFB_LOCK_H

#include "ffb_config.h"
#if ENABLE_FFB

void ffbLockInit();
void ffbLock();      // no-op if not yet initialized
void ffbUnlock();

#endif // ENABLE_FFB
#endif // FFB_LOCK_H
