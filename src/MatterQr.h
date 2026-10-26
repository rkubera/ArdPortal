// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <qrcode.h>

namespace ArdMatterQr {
// HTTP operations run serially in portal.loop(); the SDK callback is synchronous.
inline String* output = nullptr;
inline void encode(esp_qrcode_handle_t code) {
  const int size = esp_qrcode_get_size(code);
  if (!output || size < 21 || size > 57) return;
  const size_t digits = (size * size + 3) / 4;
  if (!output->reserve(digits + 32)) return;
  *output += F("{\"size\":");
  *output += String(size);
  *output += F(",\"bits\":\"");
  for (int offset = 0; offset < size * size; offset += 4) {
    unsigned value = 0;
    for (int bit = 0; bit < 4; ++bit) {
      const int pixel = offset + bit;
      value = value * 2 + (pixel < size * size && esp_qrcode_get_module(code, pixel % size, pixel / size));
    }
    *output += "0123456789abcdef"[value];
  }
  *output += F("\"}");
}
inline String generate(const String& url) {
  const int start = url.indexOf("?data=");
  if (start < 0) return String();
  String payload;
  if (!payload.reserve(url.length() - start)) return String();
  auto hex = [](char c) -> int {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                          : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                                                 : -1;
  };
  for (size_t i = start + 6; i < url.length() && url[i] != '&'; ++i) {
    char c = url[i];
    if (c == '%') {
      if (i + 2 >= url.length()) return String();
      const int high = hex(url[i + 1]), low = hex(url[i + 2]);
      if (high < 0 || low < 0) return String();
      c = char(high * 16 + low);
      i += 2;
    } else if (c == '+') c = ' ';
    payload += c;
  }
  if (payload.indexOf("MT:") != 0) return String();
  String result;
  esp_qrcode_config_t config = { encode, 10, ESP_QRCODE_ECC_MED };
  output = &result;
  const esp_err_t error = esp_qrcode_generate(&config, payload.c_str());
  output = nullptr;
  return error == ESP_OK ? result : String();
}
}
