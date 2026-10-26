// SPDX-License-Identifier: MIT
#pragma once
#include <esp_log.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

// Diagnostic builds only. SDK callbacks may run on other tasks: never enter
// ArdPortal from them, and never allocate memory or wait for a network client.
namespace ArdSdkDiagnostics {
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static char lines[4][192];
static unsigned head = 0, count = 0, dropped = 0;
/** @brief Capture bounded SDK text under the queue lock; return vsnprintf length, dropping the oldest queued record if full. */
static int capture(const char* format, va_list args) {
  char line[192] = {};
  int length = vsnprintf(line, sizeof(line), format, args);
  if (length <= 0) return length;
  portENTER_CRITICAL(&mux);
  if (count == 4) {
    head = (head + 1) % 4;
    --count;
    ++dropped;
  }
  memcpy(lines[(head + count++) % 4], line, sizeof(line));
  portEXIT_CRITICAL(&mux);
  return length;
}
/** @brief Install the SDK log sink and suppress noisy OpenThread/NimBLE INFO traces. */
static void begin() {
  esp_log_set_vprintf(capture);
  // Poll/GATT traces can flood the bounded queue and obscure CHIP failures.
  esp_log_level_set("OPENTHREAD", ESP_LOG_WARN);
  esp_log_level_set("NimBLE", ESP_LOG_WARN);
}
/** @brief Forward up to four queued records and overflow counts from the Arduino loop. */
static void drain(ArdPortal& portal) {
  for (unsigned i = 0; i < 4; ++i) {
    char line[192];
    unsigned lost;
    portENTER_CRITICAL(&mux);
    bool available = count != 0;
    if (available) {
      memcpy(line, lines[head], sizeof(line));
      head = (head + 1) % 4;
      --count;
    }
    lost = dropped;
    dropped = 0;
    portEXIT_CRITICAL(&mux);
    if (lost) portal.log(String("SDK: dropped ") + lost);
    if (!available) break;
    size_t length = strlen(line);
    while (length && (line[length - 1] == '\n' || line[length - 1] == '\r')) line[--length] = 0;
    if (length) portal.log(String("SDK: ") + line);
  }
}
}
