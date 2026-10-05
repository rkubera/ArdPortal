// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"

// Shared resource-limited ArdJSON parsing and serialization.
class ArdJsonCodec {
public:
  ArdJSON::JSONVar parse(const String& input, String* error = nullptr,
      const ArdJSON::Limits& limits = ArdJSON::Limits()) const {
    return ArdJSON::JSON.parse(input, error, limits);
  }
  String stringify(const ArdJSON::JSONVar& value, bool pretty = false,
      String* error = nullptr, const ArdJSON::Limits& limits = ArdJSON::Limits()) const {
    return ArdJSON::JSON.stringify(value, pretty, error, limits);
  }
};
