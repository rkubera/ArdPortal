// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
struct ArdAppConfigRegistrationError {
  const char* stage = "";
  String fieldId;
  String reason;
};
