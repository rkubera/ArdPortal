// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "JsonStream.h"
#include "JournalStream.h"

// Streaming reader for canonical and reordered/whitespace journal envelopes.
// Plain legacy JSON objects are recognized without retaining their contents.
struct ArdJournalReader {
  ArdJsonStream json;
  uint32_t generation = 0, expectedCrc = 0, crc = UINT32_MAX;
  size_t length = 0;
  bool legacy = false, hasJournal = false, object = false, schema = true;
  uint8_t seen = 0, field = 0, keySize = 0;
  char key[12] = {};
  uint32_t number = 0;
  bool integer = true, dataString = false, keyBad = false;
  void reset();
  static uint32_t multiply(uint32_t left, uint32_t right);
  static uint32_t combine(uint32_t left, uint32_t right, size_t bytes);
  struct Sink {
    void* context;
    bool (*invoke)(void*, uint8_t, size_t);
  };

  bool event(ArdJsonStream::Event e, uint8_t b, unsigned depth, const Sink& emit);
  template<class Emit> void input(uint8_t b, Emit& emit) {
    const Sink sink{const_cast<void*>(static_cast<const void*>(&emit)),
      [](void* context, uint8_t c, size_t at) { return (*static_cast<Emit*>(context))(c, at); }};
    inputByte(b, sink);
  }
  void inputByte(uint8_t b, const Sink& sink);
  bool finish();
private:
  struct Context { ArdJournalReader* reader; const Sink* sink; };
  static bool dispatch(void* pointer, ArdJsonStream::Event e, uint8_t b, unsigned depth);
};
