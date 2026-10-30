// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalDeclarations.h"
#include "ArdJSON.h"

// Configuration schema validation, independent of ArdFS. Parsing/encoding is generic.
namespace ArdPortalJson {
/**
 * @brief Build the JSON limits used for persistent configuration documents.
 * @return Configured JSON byte, node, string and depth limits.
 */
inline ArdJSON::Limits limits() {
  ArdJSON::Limits value;
  value.maxDepth = 1;
  value.maxNodes = 16;
  value.maxStringBytes = 4096;
  return value;
}
#if ARDPORTAL_ENABLE_WWW && ARDPORTAL_ENABLE_BACKUP_RESTORE
// XOR the low nibble of ASCII/continuation bytes; UTF-8 lead bytes stay intact.
// All UTF-8 continuation restrictions align to 16-byte ranges, so this preserves
// valid UTF-8 and byte length. The same portable operation reverses the mask.
inline bool mask(const String& text, String& result) {
  String out;
  if (text.length() && !out.reserve(text.length())) return false;
  for (size_t i = 0; i < text.length(); ++i) {
    uint8_t c = text[i];
    if (c < 0xc0) c ^= 0x0f;
    char byte = char(c);
    if (!out.concat(&byte, 1)) return false;
  }
  result = std::move(out);
  return true;
}
#endif
struct TextField {
  const char* key;
  String ArdPortal::PortalConfig::*member;
  uint16_t limit;
  bool enabled;
  bool masked = false;
};
static const TextField TextFields[] PROGMEM = {
  { "ssid", &ArdPortal::PortalConfig::ssid, 32, ARDPORTAL_ENABLE_WIFI },
  { "password", &ArdPortal::PortalConfig::password, 64, ARDPORTAL_ENABLE_WIFI, true },
  { "host", &ArdPortal::PortalConfig::host, 253, ARDPORTAL_ENABLE_MQTT },
  { "user", &ArdPortal::PortalConfig::user, 128, ARDPORTAL_ENABLE_MQTT },
  { "mqttPassword", &ArdPortal::PortalConfig::mqttPassword, 128, ARDPORTAL_ENABLE_MQTT },
  { "caCert", &ArdPortal::PortalConfig::caCert, 4096, ARDPORTAL_ENABLE_MQTT_TLS, true },
  { "apName", &ArdPortal::PortalConfig::apName, 32, ARDPORTAL_ENABLE_WIFI },
  { "apPassword", &ArdPortal::PortalConfig::apPassword, 63, ARDPORTAL_ENABLE_WIFI, true },
  { "deviceName", &ArdPortal::PortalConfig::deviceName, 32, true },
  { "deviceDescription", &ArdPortal::PortalConfig::deviceDescription, 128, true },
  { "deviceManufacturer", &ArdPortal::PortalConfig::deviceManufacturer, 128, true }
};
/**
 * @brief Copy a portal text-field descriptor from program memory.
 * @param index Zero-based element or field index.
 * @return Descriptor containing its key, config member pointer, limit and feature flag.
 */
inline TextField field(size_t index) {
  TextField entry;
  memcpy_P(&entry, TextFields + index, sizeof(entry));
  return entry;
}
/**
 * @brief Compare portal settings by their scalar and text fields.
 * @param a First value or byte range to compare.
 * @param b Second value or byte range to compare.
 * @return True if every portal setting matches; false otherwise.
 */
inline bool equal(const ArdPortal::PortalConfig& a, const ArdPortal::PortalConfig& b) {
#if ARDPORTAL_ENABLE_MQTT
  if (a.port != b.port) return false;
#endif
#if ARDPORTAL_ENABLE_MQTT_TLS
  if (a.mqttTls != b.mqttTls) return false;
#endif
#if ARDPORTAL_ENABLE_WIFI
  if (a.apBand != b.apBand) return false;
#endif
  for (size_t i = 0; i < sizeof(TextFields) / sizeof(TextFields[0]); ++i) {
    auto entry = field(i);
    if (entry.enabled && a.*entry.member != b.*entry.member) return false;
  }
  return true;
}
/**
 * @brief Build the portal configuration JSON object with version and connection settings.
 * @param config Portal settings to validate and apply.
 * @return Portal settings as a JSON object.
 */
inline ArdJSON::JSONVar value(const ArdPortal::PortalConfig& config, bool backup = false) {
  auto root = ArdJSON::JSONVar::object();
  root["version"] = 1;
#if ARDPORTAL_ENABLE_WIFI
  root["apBand"] = config.apBand;
#endif
#if ARDPORTAL_ENABLE_MQTT
  root["port"] = config.port;
#endif
#if ARDPORTAL_ENABLE_MQTT_TLS
  root["mqttTls"] = config.mqttTls;
#endif
  for (size_t i = 0; i < sizeof(TextFields) / sizeof(TextFields[0]); ++i) {
    auto entry = field(i);
    if (!entry.enabled) continue;
    const String& raw = config.*entry.member;
#if ARDPORTAL_ENABLE_WWW && ARDPORTAL_ENABLE_BACKUP_RESTORE
    if (backup && entry.masked) {
      String text;
      if (!mask(raw, text)) return ArdJSON::JSONVar();
      root[entry.key] = text;
    } else
#else
    (void)backup;
#endif
      root[entry.key] = raw;
  }
  return root;
}
/**
 * @brief Serialize portal settings as a versioned JSON document.
 * @param config Portal settings to validate and apply.
 * @param codec JSON codec used for validation and serialization.
 * @return Serialized JSON text, or an empty string when serialization fails.
 */
inline String encode(const ArdPortal::PortalConfig& config, const ArdJsonCodec& codec = ArdJsonCodec(), bool backup = false) {
  backup = backup && ARDPORTAL_ENABLE_WWW && ARDPORTAL_ENABLE_BACKUP_RESTORE;
  auto budget = limits();
  budget.maxOutputBytes = ArdPortal::MaxPortalConfigBytes * (backup ? 2 : 1);
  return codec.stringify(value(config, backup), false, nullptr, budget);
}
/**
 * @brief Extract a bounded JSON string suitable for C-string configuration APIs.
 * @param value Input value, or output destination when passed by mutable reference.
 * @param result Operation result to inspect or return.
 * @param limit Maximum allowed byte count.
 * @return True with result populated for a valid bounded string without embedded NULs.
 */
inline bool text(const ArdJSON::JSONVar& value, String& result, size_t limit, bool masked = false) {
  const String* stored = value.stringValue();
  if (!stored) return false;
  String parsed;
  if (stored->length() > limit) return false;
#if ARDPORTAL_ENABLE_WWW && ARDPORTAL_ENABLE_BACKUP_RESTORE
  if (masked) {
    if (!mask(*stored, parsed)) return false;
  } else
#else
  (void)masked;
#endif
  {
    parsed = *stored;
    if (parsed.length() != stored->length()) return false;
  }
  if (parsed.length() > limit) return false;
  // JSON allows U+0000, but the configuration feeds APIs taking C strings.
  for (size_t i = 0; i < parsed.length(); ++i)
    if (parsed[i] == 0) return false;
  const size_t length = parsed.length();
  result = std::move(parsed);
  return result.length() == length;
}
/**
 * @brief Validate a portal configuration object and copy its settings to result.
 * @param root Parsed portal configuration object.
 * @param result Operation result to inspect or return.
 * @return True with result populated if all keys, types and settings are valid; false otherwise.
 */
inline bool decodeValue(const ArdJSON::JSONVar& root, ArdPortal::PortalConfig& result, bool backup = false) {
  backup = backup && ARDPORTAL_ENABLE_WWW && ARDPORTAL_ENABLE_BACKUP_RESTORE;
  if (root.type() != ArdJSON::JSONVar::Type::Object || !root.isValid()) return false;
  int64_t version;
  if (!root["version"].toInteger(version) || version != 1) return false;
  ArdPortal::PortalConfig config;
  size_t count = 1;
  if (root.hasOwnProperty("port")) {
    ++count;
#if ARDPORTAL_ENABLE_MQTT
    int64_t port;
    if (!root["port"].toInteger(port) || port < 1 || port > 65535) return false;
    config.port = static_cast<uint16_t>(port);
#endif
  }
  if (root.hasOwnProperty("mqttTls")) {
    ++count;
#if ARDPORTAL_ENABLE_MQTT_TLS
    if (root["mqttTls"].type() != ArdJSON::JSONVar::Type::Boolean) return false;
    config.mqttTls = root["mqttTls"].asBool();
#endif
  }
  if (root.hasOwnProperty("apBand")) {
    ++count;
#if ARDPORTAL_ENABLE_WIFI
    int64_t band;
    if (!root["apBand"].toInteger(band) || (band != 24 && band != 5)) return false;
    config.apBand = uint8_t(band);
#endif
  }

  for (size_t i = 0; i < sizeof(TextFields) / sizeof(TextFields[0]); ++i) {
    auto entry = field(i);
    if (root.hasOwnProperty(entry.key)) {
      ++count;
      if (entry.enabled && !text(root[entry.key], config.*entry.member, entry.limit, backup && entry.masked)) return false;
    }
  }
  if (root.length() != count || !ArdPortal::validPortalConfig(config)) return false;
  result = std::move(config);
  return true;
}
/**
 * @brief Parse and validate a versioned portal configuration JSON document.
 * @param json Serialized portal configuration document.
 * @param result Operation result to inspect or return.
 * @param codec JSON codec used for validation and serialization.
 * @return True with result populated for a valid document; false otherwise.
 */
inline bool decode(const String& json, ArdPortal::PortalConfig& result, const ArdJsonCodec& codec = ArdJsonCodec(), bool backup = false) {
  return decodeValue(codec.parse(json, nullptr, limits()), result, backup);
}
}
