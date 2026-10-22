// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace ArdZigbeeConsole {
/** @brief Read bounded little-endian ZCL values. No input buffer is retained. */
struct Reader {
  const uint8_t* data;
  size_t size, at = 0;
  bool valid = true;
  /** @brief Consume an unsigned scalar. @param bytes Width. @return Value, or zero on truncated input. */
  uint32_t take(size_t bytes) {
    if (bytes > 4 || at > size || bytes > size - at) {
      valid = false;
      return 0;
    }
    uint32_t v = 0;
    for (size_t i = 0; i < bytes; ++i) v |= uint32_t(data[at++]) << (8 * i);
    return v;
  }
  /** @brief Decode a native scalar as JSON. @param type ZCL type. @param out Destination (32 bytes). @return True when decoded. */
  bool scalar(uint8_t type, char* out) {
    size_t width = type == 0x10 ? 1 : type >= 0x18 && type <= 0x1f ? type - 0x18 + 1
                                    : type >= 0x20 && type <= 0x27 ? type - 0x20 + 1
                                    : type >= 0x28 && type <= 0x2f ? type - 0x28 + 1
                                    : type == 0x30                 ? 1
                                    : type == 0x31                 ? 2
                                    : type == 0x39                 ? 4
                                                                   : 0;
    if (!width || width > 4) return false;
    uint32_t v = take(width);
    if (!valid) return false;
    if (type == 0x10) snprintf(out, 32, "%s", v == 0 ? "false" : v == 1 ? "true"
                                                                        : "null");
    else if (type == 0x39) {
      float number;
      memcpy(&number, &v, 4);
      if (std::isfinite(number)) snprintf(out, 32, "%.7g", double(number));
      else snprintf(out, 32, "null");
    } else if (type >= 0x28 && type <= 0x2b) {
      if (width < 4 && (v & (1u << (width * 8 - 1)))) v |= ~((1u << (width * 8)) - 1);
      snprintf(out, 32, "%ld", (long)int32_t(v));
    } else snprintf(out, 32, "%lu", (unsigned long)v);
    return true;
  }
};
/** @brief Decode application ZCL frames into bounded JSON records. @param data Borrowed ASDU. @param size ASDU length. @param cluster Cluster ID. @param emit Callback receiving JSON and transaction sequence. @return No value. */
template<class Emit> void decode(const uint8_t* data, size_t size, uint16_t cluster, Emit emit) {
  char body[96];
  if (!data || size < 3) {
    emit("{\"decoded\":false,\"reason\":\"truncated_header\"}", 0);
    return;
  }
  Reader r{ data, size };
  const uint8_t control = r.take(1);
  if (control & 4) r.take(2);
  const uint8_t sequence = r.take(1), command = r.take(1);
  if (!r.valid) {
    emit("{\"decoded\":false,\"reason\":\"truncated_header\"}", 0);
    return;
  }
  /** @brief Emit an explicit unsupported/truncated result without guessing data semantics. @return No value. */
  auto unknown = [&]() {
    snprintf(body, sizeof(body), "{\"cmd\":%u,\"decoded\":false,\"reason\":\"%s\",\"length\":%lu}", command, r.valid ? "unsupported_format" : "truncated_payload", (unsigned long)(size - r.at));
    emit(body, sequence);
  };
  if ((control & 3) == 0) {
    if (command == 0x0b) {
      const auto target = r.take(1), status = r.take(1);
      if (!r.valid) {
        unknown();
        return;
      }
      char to[24], result[12];
      if (target == 10) snprintf(to, sizeof(to), "\"report_attributes\"");
      else snprintf(to, sizeof(to), "%u", unsigned(target));
      if (status == 0) snprintf(result, sizeof(result), "\"SUCCESS\"");
      else snprintf(result, sizeof(result), "%u", unsigned(status));
      snprintf(body, sizeof(body), "{\"cmd\":\"default_response\",\"to\":%s,\"status\":%s}", to, result);
      emit(body, sequence);
      return;
    }
    if (command == 0 || command == 1 || command == 2 || command == 3 || command == 5 || command == 10) {
      const char* name = command == 0 ? "read_attributes" : command == 1  ? "read_response"
                                                          : command == 10 ? "report_attributes"
                                                                          : "write_attributes";
      while (r.at < size) {
        auto attr = r.take(2);
        if (!r.valid) {
          unknown();
          return;
        }
        if (command == 0) {
          snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"attr\":%u}", name, unsigned(attr));
          emit(body, sequence);
          continue;
        }
        if (command == 1) {
          auto status = r.take(1);
          if (!r.valid) {
            unknown();
            return;
          }
          if (status) {
            snprintf(body, sizeof(body), "{\"cmd\":\"read_response\",\"attr\":%u,\"status\":%u}", unsigned(attr), unsigned(status));
            emit(body, sequence);
            continue;
          }
        }
        const uint8_t type = r.take(1);
        char value[32];
        if (!r.valid || !r.scalar(type, value)) {
          unknown();
          return;
        }
        snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"attr\":%u,\"type\":%u,\"value\":%s}", name, unsigned(attr), type, value);
        emit(body, sequence);
      }
      return;
    }
    if (command == 4 || command == 7) {
      while (r.at < size) {
        auto status = r.take(1);
        uint32_t direction = 0, attr = 0;
        if (status) {
          if (command == 7) direction = r.take(1);
          attr = r.take(2);
        }
        if (!r.valid) {
          unknown();
          return;
        }
        snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"status\":%u,\"attr\":%u,\"direction\":%u}", command == 4 ? "write_response" : "configure_response", unsigned(status), unsigned(attr), unsigned(direction));
        emit(body, sequence);
      }
      return;
    }
    if (command == 6) {
      while (r.at < size) {
        auto direction = r.take(1), attr = r.take(2);
        if (direction == 0) {
          auto type = r.take(1), minimum = r.take(2), maximum = r.take(2);
          char change[32] = "null";
          bool analog = (type >= 0x20 && type <= 0x2f) || (type >= 0x38 && type <= 0x3a);
          if (analog && !r.scalar(type, change)) {
            unknown();
            return;
          }
          if (!r.valid) {
            unknown();
            return;
          }
          snprintf(body, sizeof(body), "{\"cmd\":\"configure_reporting\",\"attr\":%u,\"min\":%u,\"max\":%u,\"change\":%s}", unsigned(attr), unsigned(minimum), unsigned(maximum), change);
        } else if (direction == 1) {
          auto timeout = r.take(2);
          if (!r.valid) {
            unknown();
            return;
          }
          snprintf(body, sizeof(body), "{\"cmd\":\"configure_reporting\",\"attr\":%u,\"timeout\":%u}", unsigned(attr), unsigned(timeout));
        } else {
          unknown();
          return;
        }
        emit(body, sequence);
      }
      return;
    }
    if (command == 12) {
      auto start = r.take(2), maximum = r.take(1);
      if (!r.valid) {
        unknown();
        return;
      }
      snprintf(body, sizeof(body), "{\"cmd\":\"discover_attributes\",\"start\":%u,\"max\":%u}", unsigned(start), unsigned(maximum));
      emit(body, sequence);
      return;
    }
    if (command == 8) {
      while (r.at < size) {
        auto direction = r.take(1), attr = r.take(2);
        if (!r.valid) {
          unknown();
          return;
        }
        snprintf(body, sizeof(body), "{\"cmd\":\"read_reporting_config\",\"direction\":%u,\"attr\":%u}", unsigned(direction), unsigned(attr));
        emit(body, sequence);
      }
      return;
    }
    if (command == 13) {
      auto complete = r.take(1);
      if (!r.valid) {
        unknown();
        return;
      }
      snprintf(body, sizeof(body), "{\"cmd\":\"discover_response\",\"complete\":%s}", complete ? "true" : "false");
      emit(body, sequence);
      while (r.at < size) {
        auto attr = r.take(2), type = r.take(1);
        if (!r.valid) {
          unknown();
          return;
        }
        snprintf(body, sizeof(body), "{\"cmd\":\"discovered_attribute\",\"attr\":%u,\"type\":%u}", unsigned(attr), unsigned(type));
        emit(body, sequence);
      }
      return;
    }
    unknown();
    return;
  }
  if ((control & 3) != 1) {
    unknown();
    return;
  }
  const char* name = nullptr;
  uint32_t a = 0, b = 0, c = 0;
  if (cluster == 6 && command <= 2) {
    name = command == 0 ? "off" : command == 1 ? "on"
                                               : "toggle";
    snprintf(body, sizeof(body), "{\"cmd\":\"%s\"}", name);
  } else if (cluster == 8 && (command <= 3 || (command >= 4 && command <= 7))) {
    uint8_t kind = command & 3;
    name = kind == 0 ? "move_to_level" : kind == 1 ? "move_level"
                                       : kind == 2 ? "step_level"
                                                   : "stop_level";
    if (kind == 0) {
      a = r.take(1);
      b = r.take(2);
      snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"level\":%u,\"transition\":%u,\"onOff\":%s}", name, unsigned(a), unsigned(b), command >= 4 ? "true" : "false");
    } else if (kind == 3) snprintf(body, sizeof(body), "{\"cmd\":\"stop_level\",\"onOff\":%s}", command >= 4 ? "true" : "false");
    else {
      a = r.take(1);
      b = r.take(1);
      c = kind == 2 ? r.take(2) : 0;
      snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"mode\":%u,\"amount\":%u,\"transition\":%u}", name, unsigned(a), unsigned(b), unsigned(c));
    }
  } else if (cluster == 0x300 && (command == 0 || command == 3 || command == 6 || command == 7 || command == 0x40 || command == 0x43)) {
    if (command == 7) {
      a = r.take(2);
      b = r.take(2);
      c = r.take(2);
      snprintf(body, sizeof(body), "{\"cmd\":\"move_to_color\",\"x\":%u,\"y\":%u,\"transition\":%u}", unsigned(a), unsigned(b), unsigned(c));
    } else {
      bool enhanced = command == 0x40 || command == 0x43;
      a = r.take(enhanced ? 2 : 1);
      b = command == 3 ? 0 : r.take(1);
      c = r.take(2);
      snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"%s\":%u,\"%s\":%u,\"transition\":%u}", command == 3 ? "move_to_saturation" : command == 6 || command == 0x43 ? "move_to_hue_saturation"
                                                                                                                                                                   : "move_to_hue",
               command == 3 ? "saturation" : "hue", unsigned(a), command == 6 || command == 0x43 ? "saturation" : "direction", unsigned(b), unsigned(c));
    }
  } else if (cluster == 0x102 && (command <= 2 || command == 5 || command == 8)) {
    if (command <= 2) snprintf(body, sizeof(body), "{\"cmd\":\"%s\"}", command == 0 ? "open" : command == 1 ? "close"
                                                                                                            : "stop");
    else {
      a = r.take(1);
      snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"percent\":%u}", command == 5 ? "lift_percentage" : "tilt_percentage", unsigned(a));
    }
  } else if (cluster == 0x101 && command <= 2) {
    snprintf(body, sizeof(body), "{\"cmd\":\"%s\",\"pin\":\"redacted\"}", command == 0 ? "lock" : command == 1 ? "unlock"
                                                                                                               : "toggle_lock");
  } else if (cluster == 0x502 && command == 0) {
    a = r.take(1);
    b = r.take(2);
    c = r.take(1);
    auto level = r.take(1);
    if (!r.valid) {
      unknown();
      return;
    }
    snprintf(body, sizeof(body), "{\"cmd\":\"start_warning\",\"mode\":%u,\"strobe\":%s,\"level\":%u,\"seconds\":%u}", unsigned(a >> 4), (a & 4) ? "true" : "false", unsigned(a & 3), unsigned(b));
    emit(body, sequence);
    snprintf(body, sizeof(body), "{\"cmd\":\"warning_flash\",\"duty\":%u,\"level\":%u}", unsigned(c), unsigned(level));
  } else {
    unknown();
    return;
  }
  if (!r.valid) {
    unknown();
    return;
  }
  emit(body, sequence);
}
}
