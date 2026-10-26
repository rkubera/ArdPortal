// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "MatterNativeImpl.h"
#if ARDPORTAL_MATTER_LOG_SUPPORT
#include "MatterTraffic.h"
#endif
#if ARDPORTAL_ENABLE_WWW && !ARDPORTAL_MATTER_PRODUCTION
#include "MatterQr.h"
#endif

#if ARDPORTAL_MATTER_COMMAND_SUPPORT
bool ArdPortal::acceptMatterCommand(const ArdMatterFields::Command& command) {
  using namespace ArdMatterFields;
  using V = ArdJSON::JSONVar;
  if (command.field >= _dynamic.count()) return false;
  V field = _dynamic.at(command.field);
  if (!field.isValid()) return false;
  if (!ardPortalBasicControl(field["type"].asString()) && !ArdControl::normalize(field)) return false;
  String id = field["id"].asString();
  Kind mapping = kind(field);
  if (mapping == Kind::Unsupported) return false;
  V next = getAppConfigValue(id.c_str());
  if (!next.isValid()) return false;
  auto op = command.operation;
  uint32_t raw = command.value;
  if (mapping == Kind::Outlet || mapping == Kind::OnOffLight) {
    if (op != Operation::OnOff || raw > 1) return false;
    next = bool(raw);
  } else if (mapping == Kind::Dimmer) {
    if (op == Operation::Level && raw <= 254) next = fromLevel(raw, field);
    else if (op == Operation::OnOff && raw <= 1) next = raw ? (next.asDouble() > 0 ? next : field["max"]) : V(0);
    else return false;
  }
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
  else if (mapping == Kind::Light) {
    if (!ArdControl::normalize(field)) return false;
    if (op == Operation::OnOff && raw <= 1) next["state"] = ArdMatterFields::state(field, raw != 0);
    else if (op == Operation::Level && raw <= 254) next["brightness"] = round(double(raw) * 255.0 / 254.0);
    else if (op == Operation::Hue || op == Operation::Saturation || op == Operation::X || op == Operation::Y) {
      espRgbColor_t rgb{ uint8_t(next["color"]["r"].asDouble()), uint8_t(next["color"]["g"].asDouble()), uint8_t(next["color"]["b"].asDouble()) };
      if (op == Operation::Hue || op == Operation::Saturation) {
        if (raw > 254) return false;
        auto hsv = espRgbColorToHsvColor(rgb);
        if (op == Operation::Hue) hsv.h = raw;
        else hsv.s = raw;
        hsv.v = 254;
        rgb = espHsvColorToRgbColor(hsv);
      } else {
        if (raw > 65279) return false;
        auto xy = espRgbColorToXYColor(rgb);
        if (op == Operation::X) xy.x = raw;
        else xy.y = raw;
        rgb = espXYToRgbColor(255, xy.x, xy.y, false);
      }
      next["color"]["r"] = rgb.r;
      next["color"]["g"] = rgb.g;
      next["color"]["b"] = rgb.b;
    } else return false;
  }
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
  else if (mapping == Kind::Fan) {
    if (!ArdControl::normalize(field)) return false;
    if (op == Operation::Speed && raw <= 100) {
      next["percentage"] = raw;
      next["state"] = ArdMatterFields::state(field, raw != 0);
    } else if (op == Operation::FanMode && raw <= 4) {
      next["state"] = ArdMatterFields::state(field, raw != 0);
      next["percentage"] = raw == 1 ? 33 : raw == 2 ? 66
                                         : raw      ? 100
                                                    : 0;
    } else return false;
  }
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
  else if (mapping == Kind::Climate) {
    if (op == Operation::Mode) {
      const char* text = mode(raw);
      if (!text || !contains(field["modes"], text)) return false;
      next["mode"] = text;
      if (contains(field["modes"], "heat_cool")) {
        if (raw == 4) next["temperature"] = next["temperature_low"];
        else if (raw == 3) next["temperature"] = next["temperature_high"];
      }
    } else if (op == Operation::Heat || op == Operation::Cool) {
      double temperature = double(int32_t(raw)) / 100.0;
      String currentMode = next["mode"].asString();
      bool dual = contains(field["modes"], "heat_cool");
      if (dual) {
        next[op == Operation::Heat ? "temperature_low" : "temperature_high"] = temperature;
        if (next["temperature_high"].asDouble() - next["temperature_low"].asDouble() < 2.5) return false;
        if ((op == Operation::Heat && currentMode == "heat") || (op == Operation::Cool && currentMode == "cool")) next["temperature"] = temperature;
      } else if ((op == Operation::Heat && contains(field["modes"], "heat")) || (op == Operation::Cool && contains(field["modes"], "cool"))) next["temperature"] = temperature;
      else return false;
    } else return false;
  }
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
  else if (mapping == Kind::Cover) {
    if (!ArdControl::normalize(field)) return false;
    if (op == Operation::Open || op == Operation::Close || op == Operation::Stop) {
      const char* action = op == Operation::Open ? "OPEN" : op == Operation::Close ? "CLOSE"
                                                                                   : "STOP";
      for (size_t i = 0; i < field["controls"].length(); ++i)
        if (field["controls"][i]["payload"].asString() == action) return _appControls.appControl(field, i, V(action), ChangeSource::Matter);
      return false;
    }
    if (raw > 100) return false;
    if (op == Operation::Lift) next["position"] = 100 - raw;
    else if (op == Operation::Tilt) next["tilt"] = 100 - raw;
    else return false;
  }
#endif
  else
    return false;
  if (!ArdDynamicPages::validValue(field, next)) return false;
  bool persist = !field.hasOwnProperty("persist") || field["persist"].asBool();
  if (persist) {
    V patch = V::object();
    patch[id] = next;
    if (!scheduleAppPatch(std::move(patch), ChangeSource::Matter)) return false;
  }
  return _appControls.applyAppState(id, next, ChangeSource::Matter);
}

#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
void ArdPortal::serviceMatterDiagnostics() {
  auto& diagnostics = _matter._diagnostics;
  if (diagnostics.state != uint8_t(_matter.state())) {
    diagnostics.state = uint8_t(_matter.state());
    const char* message = "";
    switch (_matter.state()) {
      case ArdMatter::State::Waiting: message = ARD_LANG_NATIVE_MATTER_WAITING; break;
      case ArdMatter::State::Staging: message = ARD_LANG_NATIVE_MATTER_STAGING; break;
      case ArdMatter::State::Commissioning: message = ARD_LANG_NATIVE_MATTER_COMMISSIONING; break;
      case ArdMatter::State::Commissioned: message = ARD_LANG_NATIVE_MATTER_COMMISSIONED; break;
      case ArdMatter::State::Connected: message = ARD_LANG_NATIVE_MATTER_CONNECTED; break;
      case ArdMatter::State::Failed: message = ARD_LANG_NATIVE_MATTER_FAILED; break;
      case ArdMatter::State::Resetting: message = ARD_LANG_NATIVE_MATTER_RESETTING; break;
      case ArdMatter::State::NoEndpoints: message = "No Matter controls"; break;
    }
    String line(F(ARD_LANG_NATIVE_MATTER_PREFIX));
    line += message;
    if (_matter.state() == ArdMatter::State::Failed) line += _matter.error();
    log(line);
  }
  ArdMatterDiagnostics::Event event;
  portENTER_CRITICAL(&_matter._mux);
  const bool available = diagnostics.pop(event);
  const bool overflow = diagnostics.takeOverflow();
  portEXIT_CRITICAL(&_matter._mux);
  if (overflow) log(String(F(ARD_LANG_NATIVE_MATTER_PREFIX)) + ARD_LANG_NATIVE_MATTER_DIAGNOSTIC_OVERFLOW);
  if (!available) return;
  const char* message = "";
  switch (event) {
    case ArdMatterDiagnostics::Event::WindowOpened: message = ARD_LANG_NATIVE_MATTER_EVENT_WINDOWOPENED; break;
    case ArdMatterDiagnostics::Event::SessionStarted: message = ARD_LANG_NATIVE_MATTER_EVENT_SESSIONSTARTED; break;
    case ArdMatterDiagnostics::Event::SessionStopped: message = ARD_LANG_NATIVE_MATTER_EVENT_SESSIONSTOPPED; break;
    case ArdMatterDiagnostics::Event::Commissioned: message = ARD_LANG_NATIVE_MATTER_EVENT_COMMISSIONED; break;
    case ArdMatterDiagnostics::Event::TimedOut: message = ARD_LANG_NATIVE_MATTER_EVENT_TIMEDOUT; break;
    case ArdMatterDiagnostics::Event::WindowClosed: message = ARD_LANG_NATIVE_MATTER_EVENT_WINDOWCLOSED; break;
    case ArdMatterDiagnostics::Event::FabricRemoved: message = ARD_LANG_NATIVE_MATTER_EVENT_FABRICREMOVED; break;
    case ArdMatterDiagnostics::Event::SecureSession: message = ARD_LANG_NATIVE_MATTER_EVENT_SECURESESSION; break;
    case ArdMatterDiagnostics::Event::BleReleased: message = ARD_LANG_NATIVE_MATTER_EVENT_BLERELEASED; break;
    case ArdMatterDiagnostics::Event::FactoryReset: message = ARD_LANG_NATIVE_MATTER_EVENT_FACTORYRESET; break;
    case ArdMatterDiagnostics::Event::HandlerUnavailable: message = ARD_LANG_NATIVE_MATTER_EVENT_HANDLERUNAVAILABLE; break;
  }
  log(String(F(ARD_LANG_NATIVE_MATTER_PREFIX)) + message);
}
#endif

void ArdPortal::serviceMatter(uint32_t now) {
  using V = ArdJSON::JSONVar;
  if (!_configurationReady || otaActive() || _rebootPending) return;
  if (_matter._reset) {
    if (portalAndAppConfigBusy() || _http || _matter._state == ArdMatter::State::Resetting) return;
    _matter._state = ArdMatter::State::Resetting;
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
    serviceMatterDiagnostics();
#endif
    Matter.decommission();
    return;
  }
  if (_matter._state == ArdMatter::State::Failed) return;
#if ARDPORTAL_MATTER_LOG_SUPPORT && ARDPORTAL_MATTER_COMMAND_SUPPORT
  portENTER_CRITICAL(&_matter._mux);
  const uint8_t dropped = _matter._commandDrops;
  _matter._commandDrops = 0;
  portEXIT_CRITICAL(&_matter._mux);
  if (dropped) consoleLine(2, String("Commands rejected (device busy): ") + String(dropped));
#endif
  if (!_matter.started()) {
    if (_matter._state == ArdMatter::State::NoEndpoints && !_appConfigPageRegistration && _appConfigPageRegistrationState != AppConfigPageRegistrationState::Failed && _matter._seen == _dynamic.count()) return;
    if (_appConfigPageRegistration || _matter._seen != _dynamic.count()) {
      if (_matter.frozen()) {
        _matter.fail("Matter layout changed during endpoint staging");
        return;
      }
      _matter._seen = _dynamic.count();
      _matter._state = ArdMatter::State::Waiting;
      _matter._stableSince = now;
      return;
    }
    if (uint32_t(now - _matter._stableSince) < 1000) return;
    _matter._state = ArdMatter::State::Staging;
    if (_matter._build < _dynamic.count()) {
      size_t index = _matter._build;
      if (!ArdHeap::permits(_dynamic.fieldWork(index) + 4096, _dynamic.fieldBlock(index))) return;
      V field = _dynamic.at(index);
      if (!field.isValid() || field.isUndefined()) return;
      if (!ardPortalBasicControl(field["type"].asString()) && !ArdControl::normalize(field)) {
        _matter.fail("Invalid Matter control model");
        return;
      }
      V value = getAppConfigValue(field["id"].asString().c_str());
      if (!value.isValid()) return;
      if (_matter.add(index, field, value)) {
        auto mapping = ArdMatterFields::kind(field);
        if (mapping != ArdMatterFields::Kind::Unsupported) {
          String id = field["id"].asString();
          for (size_t i = 0; i < id.length(); ++i) _matter._layout = (_matter._layout ^ uint8_t(id[i])) * 16777619UL;
          _matter._layout = (_matter._layout ^ uint8_t(mapping)) * 16777619UL;
          uint8_t capabilities = 0;
          if (mapping == ArdMatterFields::Kind::Climate) {
            capabilities = ArdMatterFields::contains(field["modes"], "heat") | (ArdMatterFields::contains(field["modes"], "cool") << 1) | (ArdMatterFields::contains(field["modes"], "heat_cool") << 2);
          }
          _matter._layout = (_matter._layout ^ capabilities) * 16777619UL;
        }
        ++_matter._build;
      }
      return;
    }
#if ARDPORTAL_MATTER_THREAD
#if ARDPORTAL_ENABLE_WIFI
    // Let station association and AP shutdown finish before the SDK's peak.
    if (_attempting || _pendingReady || _savePending || _storage.busy()) return;
#endif
    if (!ArdHeap::permits(ARDPORTAL_ENABLE_WIFI ? 61440 : 40960, 8192)) {
      _matter._error = "Waiting for Matter startup memory";
      return;
    }
    _matter._error = "";
#endif
    if (!_matter.endpointCount() && _appConfigPageRegistrationState == AppConfigPageRegistrationState::Failed) {
      _matter.fail("App page registration failed");
      return;
    }
    _matter.start(_config.deviceName.c_str(), _config.deviceManufacturer.c_str());
    return;
  }
  _matter.refreshConnectivity();
  _matter._state = _matter.connected() ? ArdMatter::State::Connected : _matter.commissioned() ? ArdMatter::State::Commissioned
                                                                                              : ArdMatter::State::Commissioning;
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  // Same-source commands can merge into a pending patch during save debounce.
  // Wait only while the writer owns that tree or another source owns the save.
  if (!_saveQueued && !_saveRetryPending && !_pendingReady && !_deferredApp.length() && (!_savePending || _saveSource == ChangeSource::Matter) && !_httpWaitingStorage && !_storage.busy() && !mqttTrialActive()) {
    ArdMatterFields::Command command;
    if (_matter.pop(command)) {
      const bool accepted = acceptMatterCommand(command);
#if ARDPORTAL_MATTER_LOG_SUPPORT
      V definition = _dynamic.at(command.field);
      consoleLine(2, ArdMatterTraffic::received(definition, command, getAppConfigValue(definition["id"].asString().c_str()), accepted));
#else
      (void)accepted;
#endif
      _matter.dirty(command.field);  // Rejected/stale commands restore actual application state too.
    }
  }
#endif
  if (int32_t(now - _matter._reportAt) < 0 || !_matter._entries) return;
  auto* entry = _matter._report ? _matter._report : _matter._entries;
  _matter._report = entry->next;
  if (!entry->dirty) return;
  V field = _dynamic.at(entry->field);
  if (!ardPortalBasicControl(field["type"].asString()) && !ArdControl::normalize(field)) return;
  V value = getAppConfigValue(field["id"].asString().c_str());
  if (field.isValid() && value.isValid() && _matter.publish(*entry, field, value)) {
    entry->dirty = false;
#if ARDPORTAL_MATTER_LOG_SUPPORT
    consoleLine(2, ArdMatterTraffic::reported(field, value));
#endif
  }
  _matter._reportAt = now + 20;
}

#if ARDPORTAL_ENABLE_WWW
bool ArdPortal::handleMatterHttp(const String& method, const String& path) {
  using V = ArdJSON::JSONVar;
  if (path == "/api/matter/qr" && method == "GET") {
#if ARDPORTAL_MATTER_PRODUCTION
    reply(404, "text/plain", F("Use the device pairing label"));
#else
    String matrix = ArdMatterQr::generate(_matter.onboardingUrl());
    reply(matrix.length() ? 200 : 503, matrix.length() ? "application/json" : "text/plain", matrix.length() ? matrix : String(F("Matter QR code unavailable")));
#endif
    return true;
  }
  if (path == "/api/matter/status" && method == "GET") {
    V out = V::object();
    const char* states[] = { "waiting", "staging", "commissioning", "commissioned", "connected", "failed", "resetting", "no_endpoints" };
    out["state"] = states[unsigned(_matter.state())];
    out["credentials"] = ARDPORTAL_MATTER_PRODUCTION ? "production" : "development";
    out["transport"] = ARDPORTAL_MATTER_THREAD ? "thread" : "wifi";
    out["commissioned"] = _matter.commissioned();
    if (_matter.state() == ArdMatter::State::NoEndpoints) out["info"] = "No Matter controls";
    out["commissioningInProgress"] = _matter._commissioningInProgress;
    out["connected"] = _matter.connected();
#if ARDPORTAL_MATTER_THREAD
#if defined(CONFIG_ENABLE_ICD_SERVER) && CONFIG_ENABLE_ICD_SERVER
    out["threadIcd"] = true;
#else
    out["threadIcd"] = false;
#endif
    out["threadDeviceType"] = ARDPORTAL_MATTER_THREAD_ROUTER ? "ftd" : "mtd";
    out["threadProvisioned"] = _matter._threadProvisioned;
    out["threadConnected"] = _matter._running && _matter._connected;
    out["wifiConnected"] = _matter._running && _matter._wifiConnected;
    out["threadRole"] = otThreadDeviceRoleToString(otDeviceRole(_matter._threadRole));
    out["threadSrpHost"] = otSrpClientItemStateToString(otSrpClientItemState(_matter._threadSrpHost));
    out["threadSrpRunning"] = bool(_matter._threadDiscovery & 1);
    out["threadCommissionableRegistered"] = bool(_matter._threadDiscovery & 2);
    out["threadOperationalRegistered"] = bool(_matter._threadDiscovery & 4);
#if ARDPORTAL_ENABLE_WIFI
    out["wifiThreadCoexistence"] = _matter.started();
    out["threadUdpBound"] = _matter.started();
#endif
#endif
    out["endpoints"] = _matter.endpointCount();
    out["unsupported"] = _matter.unsupportedCount();
    out["error"] = _matter.error();
    out["manualCode"] = _matter.manualPairingCode();
    out["qrUrl"] = _matter.onboardingUrl();
    reply(200, "application/json", _json.stringify(out));
    return true;
  }
  if (path == "/api/matter/pair" || path == "/api/matter/reset") {
    if (method != "POST") {
      reply(405, "text/plain", F("Use POST"));
      return true;
    }
    bool reset = path == "/api/matter/reset";
    if (reset && _request.substring(_request.indexOf("\r\n\r\n") + 4) != "confirm=RESET-MATTER") {
      reply(400, "text/plain", F("Matter reset confirmation required"));
      return true;
    }
    bool allowed = _configurationReady && !portalAndAppConfigBusy() && !otaActive() && !_rebootPending;
    bool accepted = allowed && (reset ? _matter.reset() : _matter.openCommissioningWindow());
    reply(accepted ? 202 : 409, "text/plain", accepted ? F("Matter operation accepted") : F("Matter is not ready or busy"));
    return true;
  }
  return false;
}
#endif
