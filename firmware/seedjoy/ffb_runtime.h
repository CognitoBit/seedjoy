/**
 * SeedJoy - Force Feedback runtime (report parsing + PID handshake)
 *
 * Bridges USB PID reports to the pure FfbEngine. Deliberately free of Arduino /
 * TinyUSB includes so it is unit-tested on the host (firmware/tests): the USB
 * layer hands it de-multiplexed (reportId, bytes) and asks it to fill GET
 * responses. It never touches hardware — the engine's force output is consumed
 * by the 1 kHz driver (ffb_runtime is the "what", the driver is the "when").
 */
#ifndef FFB_RUNTIME_H
#define FFB_RUNTIME_H

#include "ffb_engine.h"
#include "ffb_reports.h"

// Nominal RAM per effect block, reported to the host in Pool / Block Load.
// The pool is device-managed, so this is bookkeeping the host mostly ignores.
#define FFB_BLOCK_RAM 0x10

class FfbRuntime {
public:
  FfbRuntime();

  FfbEngine&       engine()       { return engine_; }
  const FfbEngine& engine() const { return engine_; }

  // Host -> device output report (report-id byte already stripped).
  void handleOutputReport(uint8_t reportId, const uint8_t* data, uint16_t len,
                          uint32_t now_ms);

  // Host -> device SET feature (Create New Effect).
  void handleSetFeature(uint8_t reportId, const uint8_t* data, uint16_t len);

  // Device -> host GET feature (Block Load, Pool). Fills buffer, returns length
  // written (0 if the report id is not a feature we serve).
  uint16_t handleGetFeature(uint8_t reportId, uint8_t* buffer, uint16_t reqlen);

  // Build the PID State input report payload (ID 2). Returns length.
  uint16_t buildStateReport(uint8_t* buffer, uint16_t maxlen);

  // Introspection for tests / diagnostics.
  uint8_t lastCreatedBlock() const { return pendingBlockIndex_; }
  uint8_t lastLoadStatus()   const { return pendingLoadStatus_; }

private:
  FfbEngine engine_;

  // Create New Effect -> Block Load handshake result.
  uint8_t pendingBlockIndex_;
  uint8_t pendingLoadStatus_;
};

#endif // FFB_RUNTIME_H
