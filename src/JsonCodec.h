// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdJSON.h"

// Shared resource-limited ArdJSON parsing and serialization.
class ArdJsonCodec {
public:
  /**
   * @brief Parse JSON text with the supplied resource limits.
   * @param input Source text or bytes to process.
   * @param error Output error text; populated when the operation fails.
   * @param limits JSON parsing or serialization resource limits.
   * @return Parsed JSON value, or Undefined with error text when input is invalid or a resource limit is exceeded.
   */
  ArdJSON::JSONVar parse(const String& input, String* error = nullptr,
      const ArdJSON::Limits& limits = ArdJSON::Limits()) const {
    return ArdJSON::JSON.parse(input, error, limits);
  }
  /**
   * @brief Serialize a JSON value with the supplied resource limits.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param pretty Whether to indent the serialized JSON.
   * @param error Output error text; populated when the operation fails.
   * @param limits JSON parsing or serialization resource limits.
   * @return Serialized JSON text, or an empty string with error text on failure.
   */
  String stringify(const ArdJSON::JSONVar& value, bool pretty = false,
      String* error = nullptr, const ArdJSON::Limits& limits = ArdJSON::Limits()) const {
    return ArdJSON::JSON.stringify(value, pretty, error, limits);
  }
};
