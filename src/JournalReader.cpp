// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "JournalReader.h"
#include "ArdCRC.h"

void ArdJournalReader::reset() {
    json.reset(); generation = expectedCrc = 0; crc = UINT32_MAX; length = 0;
    legacy = hasJournal = object = false; schema = integer = true;
    seen = field = keySize = 0; number = 0; dataString = keyBad = false;
  }

uint32_t ArdJournalReader::multiply(uint32_t left, uint32_t right) {
  return ArdCRC::multiply(left, right);
}
uint32_t ArdJournalReader::combine(uint32_t left, uint32_t right, size_t bytes) {
  return ArdCRC::combine(left, right, bytes);
}

bool ArdJournalReader::event(ArdJsonStream::Event e, uint8_t b, unsigned depth, const Sink& emit) {
    using E = ArdJsonStream::Event;
    if (depth == 0 && e == E::Object) object = true;
    if (depth != 1) return true;
    if (e == E::Key) { keySize = 0; field = 0; keyBad = false; }
    else if (e == E::KeyByte) { if (!b || keySize >= sizeof(key) - 1) keyBad = true; else key[keySize++] = char(b); }
    else if (e == E::EndKey) {
      key[keySize] = 0;
      field = keyBad ? 0 : !strcmp(key, "journal") ? 1 : !strcmp(key, "generation") ? 2 : !strcmp(key, "crc32") ? 4 : !strcmp(key, "data") ? 8 : 0;
      if (field == 1) hasJournal = true;
      if (!field || (seen & field)) schema = false;
      seen |= field;
    } else if (e == E::Number) { number = 0; integer = field && field != 8; }
    else if (e == E::NumberByte) {
      if (b < '0' || b > '9' || number > 429496729U || (number == 429496729U && b > '5')) integer = false;
      else number = number * 10 + b - '0';
    } else if (e == E::EndNumber) {
      if (!integer) schema = false;
      else if (field == 1) schema &= number == 1;
      else if (field == 2) generation = number;
      else if (field == 4) expectedCrc = number;
    } else if (e == E::String) { dataString = field == 8; if (!dataString) schema = false; }
    else if (e == E::StringByte && dataString) {
      crc = ArdJournalStream::update(crc, b);
      size_t at = length++;
      if (emit.invoke && !emit.invoke(emit.context, b, at)) return false;
    } else if (e == E::EndString) dataString = false;
    else if (e == E::Object || e == E::Array || e == E::Literal) schema = false;
    return true;
  }

bool ArdJournalReader::finish() {
    const Sink sink{nullptr, nullptr};
    Context context{this, &sink};
    const ArdJsonStream::Listener listener{&context, dispatch};
    if (!json.finishInput(listener) || !object) return false;
    legacy = !hasJournal;
    if (legacy) return true;
    if (!schema || seen != 15) return false;
    uint32_t prefix = UINT32_MAX;
    for (unsigned i = 0; i < 4; ++i) prefix = ArdJournalStream::update(prefix, uint8_t(generation >> (8 * i)));
    return combine(~prefix, ~crc, length) == expectedCrc;
  }

bool ArdJournalReader::dispatch(void* pointer, ArdJsonStream::Event e, uint8_t b, unsigned depth) {
  auto& context = *static_cast<Context*>(pointer);
  return context.reader->event(e, b, depth, *context.sink);
}
void ArdJournalReader::inputByte(uint8_t b, const Sink& sink) {
  Context context{this, &sink};
  const ArdJsonStream::Listener listener{&context, dispatch};
  json.inputByte(b, listener);
}
