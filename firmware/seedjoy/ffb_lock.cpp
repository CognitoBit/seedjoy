/**
 * SeedJoy - Force Feedback concurrency lock (FreeRTOS mutex)
 */
#include "ffb_config.h"
#if ENABLE_FFB

#include "ffb_lock.h"
#include <Arduino.h>          // Adafruit nRF52 core is FreeRTOS-based
#include <FreeRTOS.h>
#include <semphr.h>

static SemaphoreHandle_t s_ffbMutex = NULL;

void ffbLockInit() {
  if (!s_ffbMutex) {
    s_ffbMutex = xSemaphoreCreateMutex();   // priority-inheriting
  }
}

void ffbLock() {
  // Block until acquired. The only holder that matters is the loop tick, which
  // holds it for tens of microseconds of pure math, so the usbd task's wait is
  // short and bounded (priority inheritance boosts the loop while it holds it).
  if (s_ffbMutex) xSemaphoreTake(s_ffbMutex, portMAX_DELAY);
}

void ffbUnlock() {
  if (s_ffbMutex) xSemaphoreGive(s_ffbMutex);
}

#endif // ENABLE_FFB
