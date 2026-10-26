// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// SDK callbacks only copy event IDs. The portal formats messages from loop().
// The caller protects push/pop with the existing Matter critical section.
class ArdMatterDiagnostics {
public:
  enum class Event : uint8_t {
    WindowOpened,
    SessionStarted,
    SessionStopped,
    Commissioned,
    TimedOut,
    WindowClosed,
    FabricRemoved,
    SecureSession,
    BleReleased,
    FactoryReset,
    HandlerUnavailable
  };
  uint8_t state = 255;
  /** @brief Append an event ID or latch overflow when full; caller must hold the Matter critical section. */
  void push(Event event) {
    if (_count == sizeof(_events)) {
      _overflow = true;
      return;
    }
    _events[(_head + _count++) % sizeof(_events)] = event;
  }
  /** @brief Remove the oldest event into event; false when empty. Caller protects access with the Matter critical section. */
  bool pop(Event& event) {
    if (!_count) return false;
    event = _events[_head];
    _head = (_head + 1) % sizeof(_events);
    --_count;
    return true;
  }
  /** @brief Return and clear the overflow latch under caller-provided synchronization. */
  bool takeOverflow() {
    const bool value = _overflow;
    _overflow = false;
    return value;
  }
private:
  Event _events[8]{};
  uint8_t _head = 0, _count = 0;
  bool _overflow = false;
};
