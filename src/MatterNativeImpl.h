// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdMatter.h"
#include "MatterLabels.h"
#include <type_traits>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>
#include <nvs.h>
#if ARDPORTAL_MATTER_THREAD && ARDPORTAL_ENABLE_WIFI
#include "MatterThreadTransport.h"
#endif
#if ARDPORTAL_MATTER_THREAD && ARDPORTAL_ENABLE_WIFI && CONFIG_ESP_COEX_SW_COEXIST_ENABLE && CONFIG_SOC_IEEE802154_SUPPORTED
#include <esp_coexist.h>
#endif
#if ARDPORTAL_MATTER_THREAD && ARDPORTAL_ENABLE_WWW
#include <esp_openthread.h>
#include <esp_openthread_lock.h>
#include <openthread/srp_client.h>
#include <openthread/thread.h>
#endif
#if ARDPORTAL_MATTER_PRODUCTION
#include "MatterFactory.h"
#endif
#include <app/server/CommissioningWindowManager.h>

// Shared by every endpoint adapter: do not duplicate this switch per Native<T>.
inline __attribute__((noinline)) int ArdMatter::prepareCommand(uint32_t cluster, uint32_t attribute, const esp_matter_attr_val_t* value, Operation& operation, uint32_t& raw) {
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  if (_localReport) return 0;
  (void)attribute;
  (void)value;
  (void)operation;
  (void)raw;
  bool command = true;
  switch (cluster) {
#if ARDPORTAL_ENABLE_CONTROL_SWITCH || ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_LIGHT || ARDPORTAL_ENABLE_CONTROL_FAN
    case 6:
      operation = Operation::OnOff;
      raw = value->val.b;
      command = attribute == 0;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER || ARDPORTAL_ENABLE_CONTROL_LIGHT
    case 8:
      operation = Operation::Level;
      raw = value->val.u8;
      command = attribute == 0;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
    case 0x300:
      if (attribute == 0) {
        operation = Operation::Hue;
        raw = value->val.u8;
      } else if (attribute == 1) {
        operation = Operation::Saturation;
        raw = value->val.u8;
      } else if (attribute == 3) {
        operation = Operation::X;
        raw = value->val.u16;
      } else if (attribute == 4) {
        operation = Operation::Y;
        raw = value->val.u16;
      } else command = false;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
    case 0x202:
      operation = attribute == 0 ? Operation::FanMode : Operation::Speed;
      raw = value->val.u8;
      command = attribute == 0 || attribute == 2;
      break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    case 0x201:
      if (attribute == 0x1c) {
        operation = Operation::Mode;
        raw = value->val.u8;
      } else if (attribute == 0x11 || attribute == 0x12) {
        operation = attribute == 0x11 ? Operation::Cool : Operation::Heat;
        raw = uint32_t(int32_t(value->val.i16));
      } else command = false;
      break;
#endif
    default: command = false; break;
  }
  if (!command) return 0;
  portENTER_CRITICAL(&_mux);
  const bool available = !_accepting || _queued < 8;
#if ARDPORTAL_MATTER_LOG_SUPPORT
  if (!available && _commandDrops != 255) ++_commandDrops;
#endif
  portEXIT_CRITICAL(&_mux);
  return available ? 1 : -1;
#else
  (void)cluster;
  (void)attribute;
  (void)value;
  (void)operation;
  (void)raw;
  return 0;
#endif
}

template<class T> struct ArdMatter::Native : T {
  Entry* entry;
  explicit Native(Entry* e)
    : entry(e) {}
  bool attributeChangeCB(uint16_t endpoint, uint32_t cluster, uint32_t attribute, esp_matter_attr_val_t* value) override {
    auto* owner = entry->owner;
    if (!owner) return false;
    // Read-only sensors and covers never consume these attribute commands.
    if constexpr (
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
      std::is_same<T, MatterOnOffPlugin>::value || std::is_same<T, MatterOnOffLight>::value ||
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
      std::is_same<T, MatterDimmablePlugin>::value ||
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
      std::is_same<T, MatterColorLight>::value ||
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
      std::is_same<T, MatterFan>::value ||
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
      std::is_same<T, MatterThermostat>::value ||
#endif
      false) {
      Operation operation{};
      uint32_t raw = 0;
      const int command = owner->prepareCommand(cluster, attribute, value, operation, raw);
      if (command < 0) return false;
      if (command) {
        if (!T::attributeChangeCB(endpoint, cluster, attribute, value)) return false;
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
        if constexpr (std::is_same<T, MatterDimmablePlugin>::value) {
          // Matter retains a level while off; a level-only update must not turn it on.
          if (operation == Operation::Level) raw = this->getOnOff() ? raw : 0;
          else if (operation == Operation::OnOff && raw) {
            operation = Operation::Level;
            raw = this->getLevel();
          }
        }
#endif
        return owner->enqueue(*entry, operation, raw);
      }
    }
    return T::attributeChangeCB(endpoint, cluster, attribute, value);
  }
};

inline ArdMatter::~ArdMatter() {
  // ESP-Matter has no node shutdown API. Keep SDK private data alive until reboot.
  if (_running) chip::DeviceLayer::PlatformMgr().LockChipStack();
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  if (_diagnosticRegistered) chip::DeviceLayer::PlatformMgr().RemoveEventHandler(diagnosticEvent, reinterpret_cast<intptr_t>(this));
#endif
  portENTER_CRITICAL(&_mux);
  _accepting = false;
  for (Entry* entry = _entries; entry; entry = entry->next) entry->owner = nullptr;
  portEXIT_CRITICAL(&_mux);
  if (_running) chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
inline void ArdMatter::diagnosticEvent(const chip::DeviceLayer::ChipDeviceEvent* event, intptr_t context) {
  using E = ArdMatterDiagnostics::Event;
  using namespace chip::DeviceLayer;
  E message;
  switch (event->Type) {
    case DeviceEventType::kCommissioningWindowOpened: message = E::WindowOpened; break;
    case DeviceEventType::kCommissioningSessionStarted: message = E::SessionStarted; break;
    case DeviceEventType::kCommissioningSessionStopped: message = E::SessionStopped; break;
    case DeviceEventType::kCommissioningComplete: message = E::Commissioned; break;
    case DeviceEventType::kFailSafeTimerExpired: message = E::TimedOut; break;
    case DeviceEventType::kCommissioningWindowClosed: message = E::WindowClosed; break;
    case DeviceEventType::kFabricRemoved: message = E::FabricRemoved; break;
    case DeviceEventType::kSecureSessionEstablished: message = E::SecureSession; break;
    case DeviceEventType::kBLEDeinitialized: message = E::BleReleased; break;
    case DeviceEventType::kFactoryReset: message = E::FactoryReset; break;
    default: return;
  }
  auto* owner = reinterpret_cast<ArdMatter*>(context);
  portENTER_CRITICAL(&owner->_mux);
  owner->_diagnostics.push(message);
  portEXIT_CRITICAL(&owner->_mux);
}
#endif

inline bool ArdMatter::configure(const Options& options) {
  if (frozen()) return false;
#if !ARDPORTAL_MATTER_PRODUCTION
  if (options.discriminator > 4095) return false;
#endif
  _options = options;
  return true;
}
inline bool ArdMatter::enqueue(Entry& entry, Operation operation, uint32_t value) {
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  portENTER_CRITICAL(&_mux);
  bool accepted = !_accepting || _queued < 8;
#if ARDPORTAL_MATTER_LOG_SUPPORT
  if (!accepted && _commandDrops != 255) ++_commandDrops;
#endif
  if (_accepting && accepted) {
    _commands[(_head + _queued) % 8] = { entry.field, operation, value };
    ++_queued;
  }
  portEXIT_CRITICAL(&_mux);
  return accepted;
#else
  (void)entry;
  (void)operation;
  (void)value;
  return false;
#endif
}
inline bool ArdMatter::pop(Command& command) {
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  portENTER_CRITICAL(&_mux);
  bool available = _queued != 0;
  if (available) {
    command = _commands[_head];
    _head = (_head + 1) % 8;
    --_queued;
  }
  portEXIT_CRITICAL(&_mux);
  return available;
#else
  (void)command;
  return false;
#endif
}
inline void ArdMatter::discard(uint16_t field) {
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  portENTER_CRITICAL(&_mux);
  uint8_t kept = 0;
  for (uint8_t i = 0; i < _queued; ++i) {
    auto command = _commands[(_head + i) % 8];
    if (command.field != field) _commands[(_head + kept++) % 8] = command;
  }
  _queued = kept;
  portEXIT_CRITICAL(&_mux);
#else
  (void)field;
#endif
}
inline void ArdMatter::dirty(uint16_t field) {
  for (Entry* entry = _entries; entry; entry = entry->next)
    if (entry->field == field) entry->dirty = true;
}
inline bool ArdMatter::commissioned() const {
  return _running && _commissioned;
}
inline void ArdMatter::refreshConnectivity() {
  // Snapshot once per second from loop; avoid taking the CHIP/Thread locks on
  // every HTTP/storage iteration. A failed try-lock retries on the next pass.
  if (int32_t(millis() - _networkAt) < 0) return;
  if (!chip::DeviceLayer::PlatformMgr().TryLockChipStack()) return;
  _commissioned = Matter.isDeviceCommissioned();
#if ARDPORTAL_ENABLE_WWW
  _commissioningInProgress = chip::Server::GetInstance().GetFailSafeContext().IsFailSafeArmed();
#endif
  _connected = ARDPORTAL_MATTER_THREAD ? Matter.isThreadConnected() : Matter.isWiFiConnected();
#if ARDPORTAL_MATTER_THREAD
  _threadProvisioned = chip::DeviceLayer::ConnectivityMgr().IsThreadProvisioned();
  _wifiConnected = Matter.isWiFiConnected();
#endif
  chip::DeviceLayer::PlatformMgr().UnlockChipStack();
#if ARDPORTAL_MATTER_THREAD && ARDPORTAL_ENABLE_WWW
  if (esp_openthread_lock_acquire(0)) {
    if (auto* instance = esp_openthread_get_instance()) {
      _threadRole = uint8_t(otThreadGetDeviceRole(instance));
      _threadSrpHost = uint8_t(otSrpClientGetHostInfo(instance)->mState);
      _threadDiscovery = otSrpClientIsRunning(instance) ? 1 : 0;
      for (auto* service = otSrpClientGetServices(instance); service; service = service->mNext) {
        if (service->mState != OT_SRP_CLIENT_ITEM_STATE_REGISTERED) continue;
        if (!strcmp(service->mName, "_matterc._udp")) _threadDiscovery |= 2;
        if (!strcmp(service->mName, "_matter._tcp") || !strcmp(service->mName, "_matter._udp")) _threadDiscovery |= 4;
      }
    }
    esp_openthread_lock_release();
  }
#endif
  _networkAt = millis() + 1000;
}
inline bool ArdMatter::connected() const {
  return _running && _connected && _commissioned;
}
// Called from loop; SDK timers and the RTOS remain responsible for radio/CPU sleep.
inline bool ArdMatter::powerSaving(bool selected, bool busy) {
  if (!_running) return false;
#if defined(CONFIG_ENABLE_ICD_SERVER) && CONFIG_ENABLE_ICD_SERVER
  bool pending = false;
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  portENTER_CRITICAL(&_mux);
  pending = _queued != 0;
  portEXIT_CRITICAL(&_mux);
#endif
  for (Entry* entry = _entries; entry && !pending; entry = entry->next) pending = entry->dirty;
  const bool awake = !selected || busy || pending || !connected();
  if (awake && (!_powerActiveAt || uint32_t(millis() - _powerActiveAt) >= 500)) {
    if (!chip::DeviceLayer::PlatformMgr().TryLockChipStack()) return false;
    auto& manager = chip::Server::GetInstance().GetICDManager();
    manager.OnNetworkActivity();
    manager.StayActiveRequest(1000);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    _powerActiveAt = millis();
  }
  return selected && !awake;
#elif !ARDPORTAL_MATTER_THREAD
  (void)busy;
  return selected && connected();
#else
  (void)selected;
  (void)busy;
  return false;
#endif
}
inline String ArdMatter::manualPairingCode() const {
  // Arduino caches these immutable strings when the stack starts. A printed
  // setup code remains valid after the commissioning window has closed.
#if ARDPORTAL_MATTER_PRODUCTION
  // Factory commissioning uses a SPAKE2+ verifier; no setup PIN is kept here.
  return String();
#else
  return _running ? Matter.getManualPairingCode() : String();
#endif
}
inline String ArdMatter::onboardingUrl() const {
#if ARDPORTAL_MATTER_PRODUCTION
  return String();
#else
  return _running ? Matter.getOnboardingQRCodeUrl() : String();
#endif
}

inline bool ArdMatter::openCommissioningWindow(uint16_t seconds) {
  if (!_running || seconds < 180 || seconds > 900 || _reset) return false;
  if (!chip::DeviceLayer::PlatformMgr().TryLockChipStack()) return false;
  auto result = chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow(chip::System::Clock::Seconds16(seconds));
  chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  return result == CHIP_NO_ERROR;
}
inline bool ArdMatter::reset() {
  if (!_running || _reset) return false;
  nvs_handle_t handle;
  if (nvs_open("ard-matter", NVS_READWRITE, &handle) != ESP_OK) return false;
  esp_err_t result = nvs_erase_key(handle, "layout");
  if (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND) result = nvs_commit(handle);
  nvs_close(handle);
  if (result != ESP_OK) return false;
  _reset = true;
  return true;
}

inline bool ArdMatter::add(uint16_t field, const V& definition, const V& value) {
  Kind kind = ArdMatterFields::kind(definition);
  if (kind == Kind::Unsupported) {
    ++_unsupported;
    return true;
  }
  if (_count >= ARDPORTAL_MATTER_MAX_ENDPOINTS) {
    fail("Matter endpoint limit exceeded");
    return false;
  }
  if (!_selected) {
#if ARDPORTAL_MATTER_PRODUCTION
    if (!ArdMatterFactory::ready()) {
      fail("Matter factory credentials missing or incomplete");
      return false;
    }
#endif
    if (!Matter.selectNetwork(ARDPORTAL_MATTER_THREAD ? MATTER_NETWORK_THREAD : MATTER_NETWORK_WIFI)
#if !ARDPORTAL_MATTER_PRODUCTION
        || !Matter.setSetupDiscriminator(_options.discriminator) || !Matter.setSetupPasscode(_options.passcode)
#endif
        || !Matter.setBLECommissioningEnabled(_options.bleCommissioning)) {
      fail("Matter network or commissioning configuration unavailable");
      return false;
    }
    _selected = true;
  }
  Entry* entry = new (std::nothrow) Entry{ this, nullptr, nullptr, field, kind };
  if (!entry) {
    fail("Out of memory creating Matter endpoint");
    return false;
  }
  bool created = false;
#define ARD_MATTER_CREATE(TYPE, ...) \
  do { \
    auto* native = new (std::nothrow) Native<TYPE>(entry); \
    entry->endpoint = native; \
    if (native) created = native->begin(__VA_ARGS__); \
  } while (0)
  switch (kind) {
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
    case Kind::Outlet: ARD_MATTER_CREATE(MatterOnOffPlugin, value.asBool()); break;
    case Kind::OnOffLight: ARD_MATTER_CREATE(MatterOnOffLight, value.asBool()); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
    case Kind::Dimmer: ARD_MATTER_CREATE(MatterDimmablePlugin, value.asDouble() > 0, ArdMatterFields::level(value.asDouble(), definition["max"].asDouble())); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_TEXT
    case Kind::Temperature: ARD_MATTER_CREATE(MatterTemperatureSensor, 0.0); break;
    case Kind::Humidity: ARD_MATTER_CREATE(MatterHumiditySensor, 0.0); break;
    case Kind::Pressure: ARD_MATTER_CREATE(MatterPressureSensor, 0.0); break;
    // The SDK's public begin(double) accepts lux >= 1, unlike raw reports,
    // where zero correctly represents a reading below the measurable range.
    case Kind::Illuminance: ARD_MATTER_CREATE(MatterLightSensor, 1.0); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
    case Kind::Contact: ARD_MATTER_CREATE(MatterContactSensor); break;
    case Kind::Occupancy: ARD_MATTER_CREATE(MatterOccupancySensor, value.asBool()); break;
    case Kind::Leak: ARD_MATTER_CREATE(MatterWaterLeakDetector); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
    case Kind::Light: ARD_MATTER_CREATE(MatterColorLight, value["state"].asString() == ArdMatterFields::state(definition, true)); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
    case Kind::Fan: ARD_MATTER_CREATE(MatterFan, 0, MatterFan::FAN_MODE_OFF, MatterFan::FAN_MODE_SEQ_OFF_LOW_MED_HIGH); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    case Kind::Climate:
      {
        bool heat = ArdMatterFields::contains(definition["modes"], "heat") || ArdMatterFields::contains(definition["modes"], "heat_cool");
        bool cool = ArdMatterFields::contains(definition["modes"], "cool") || ArdMatterFields::contains(definition["modes"], "heat_cool");
        if (!heat && !cool) break;
        ARD_MATTER_CREATE(MatterThermostat, heat && cool ? MatterThermostat::THERMOSTAT_SEQ_OP_COOLING_HEATING : heat ? MatterThermostat::THERMOSTAT_SEQ_OP_HEATING
                                                                                                                      : MatterThermostat::THERMOSTAT_SEQ_OP_COOLING,
                          ArdMatterFields::contains(definition["modes"], "heat_cool") ? MatterThermostat::THERMOSTAT_AUTO_MODE_ENABLED : MatterThermostat::THERMOSTAT_AUTO_MODE_DISABLED);
        break;
      }
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
    case Kind::Cover:
      {
        ARD_MATTER_CREATE(MatterWindowCovering, 100, 100, MatterWindowCovering::BLIND_LIFT_AND_TILT);
        if (created) {
          auto* native = static_cast<Native<MatterWindowCovering>*>(entry->endpoint);
          using Status = chip::app::Clusters::WindowCovering::ConfigStatus;
          auto status = esp_matter_bitmap8(uint8_t(Status::kOperational) | uint8_t(Status::kLiftPositionAware) | uint8_t(Status::kTiltPositionAware));
          esp_matter_attr_val_t previousStatus{};
          created = native->getAttributeVal(0x102, 7, &previousStatus) && (previousStatus.val.u8 == status.val.u8 || native->setAttributeVal(0x102, 7, &status));
          native->onOpen([entry]() {
            return entry->owner && entry->owner->enqueue(*entry, Operation::Open, 0);
          });
          native->onClose([entry]() {
            return entry->owner && entry->owner->enqueue(*entry, Operation::Close, 0);
          });
          native->onStop([entry]() {
            return entry->owner && entry->owner->enqueue(*entry, Operation::Stop, 0);
          });
          native->onGoToLiftPercentage([entry](uint8_t p) {
            return entry->owner && entry->owner->enqueue(*entry, Operation::Lift, p);
          });
          native->onGoToTiltPercentage([entry](uint8_t p) {
            return entry->owner && entry->owner->enqueue(*entry, Operation::Tilt, p);
          });
        }
        break;
      }
#endif
    default: break;
  }
#undef ARD_MATTER_CREATE
  // Once handed to the SDK, callback private data must survive until reboot.
  entry->next = _entries;
  _entries = entry;
  if (!created) {
    fail("Matter endpoint creation failed; restart required");
    return false;
  }
  String label = definition["name"].asString();
  if (!label.length()) label = definition["id"].asString();
  if (label.length()) {
    const uint16_t endpointId = entry->endpoint->getEndPointId();
    entry->label = new (std::nothrow) ArdMatterLabel(endpointId, label.c_str());
    if (!entry->label || !esp_matter::cluster::fixed_label::create(esp_matter::endpoint::get(endpointId), nullptr, CLUSTER_FLAG_SERVER)) {
      fail("Matter endpoint label creation failed; restart required");
      return false;
    }
  }
  ++_count;
  _report = _entries;
  return true;
}

inline bool ArdMatter::start(const char* name, const char* manufacturer) {
  if (!_count) {
    _error = "";
    _state = State::NoEndpoints;
    return false;
  }
  nvs_handle_t handle;
  if (nvs_open("ard-matter", NVS_READWRITE, &handle) != ESP_OK) {
    fail("Matter layout NVS unavailable");
    return false;
  }
  uint32_t previous = 0;
  esp_err_t result = nvs_get_u32(handle, "layout", &previous);
  if (result == ESP_OK && previous != _layout) {
    nvs_close(handle);
    fail("Matter layout changed: reset Matter using previous firmware or erase NVS");
    return false;
  }
  if (result != ESP_OK && result != ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    fail("Matter layout NVS read failed");
    return false;
  }
  result = nvs_set_u32(handle, "layout", _layout);
  if (result == ESP_OK) result = nvs_commit(handle);
  nvs_close(handle);
  if (result != ESP_OK) {
    fail("Matter layout NVS write failed");
    return false;
  }
  Matter.setDeviceName(name);
#if !ARDPORTAL_MATTER_PRODUCTION
  Matter.setProductName("ArdPortal");
  Matter.setVendorName(manufacturer);
#else
  (void)manufacturer;  // Production vendor/product identity comes from fctry.
#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  // Register independently: preserve application callbacks installed via Matter.onEvent().
  _diagnosticRegistered = chip::DeviceLayer::PlatformMgr().AddEventHandler(diagnosticEvent, reinterpret_cast<intptr_t>(this)) == CHIP_NO_ERROR;
  if (!_diagnosticRegistered) _diagnostics.push(ArdMatterDiagnostics::Event::HandlerUnavailable);
#endif
  Matter.begin();
  struct SDK : ArduinoMatter {
    static bool ready() {
      return isStackStarted();
    }
  };
  if (!SDK::ready()) {
    fail("Matter stack failed to start");
    return false;
  }
#if ARDPORTAL_MATTER_THREAD && ARDPORTAL_ENABLE_WIFI
  chip::DeviceLayer::PlatformMgr().LockChipStack();
  auto& transport = static_cast<chip::ServerTransportMgr&>(chip::Server::GetInstance().GetTransportManager());
  const uint16_t port = transport.GetTransport().GetImplAtIndex<0>().GetBoundPort();
  chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  if (!ArdMatterThreadTransport::bind(port)) {
    fail("Matter Thread UDP binding failed");
    return false;
  }
#endif
  // OpenThread initialization alone does not enable Wi-Fi/802.15.4 arbitration.
#if ARDPORTAL_MATTER_THREAD && ARDPORTAL_ENABLE_WIFI
#if CONFIG_ESP_COEX_SW_COEXIST_ENABLE && CONFIG_SOC_IEEE802154_SUPPORTED
  if (esp_coex_wifi_i154_enable() != ESP_OK) {
    fail("Wi-Fi/Thread radio coexistence failed");
    return false;
  }
#else
  fail("Wi-Fi/Thread radio coexistence unavailable in SDK");
  return false;
#endif
#endif
  _running = true;
  // The SDK registers its default Fixed Label server during begin(). Replace
  // only these endpoint servers, leaving factory/device-info providers intact.
  chip::DeviceLayer::PlatformMgr().LockChipStack();
  // Restore the actual persisted state before publishing the first diagnostic.
  _commissioned = Matter.isDeviceCommissioned();
  _connected = ARDPORTAL_MATTER_THREAD ? Matter.isThreadConnected() : Matter.isWiFiConnected();
  auto& registry = esp_matter::data_model::provider::get_instance().registry();
  bool labelsReady = true;
  for (Entry* entry = _entries; entry; entry = entry->next) {
    if (!entry->label) continue;
    auto* previous = registry.Get({ entry->endpoint->getEndPointId(), chip::app::Clusters::FixedLabel::Id });
    if ((previous && registry.Unregister(previous) != CHIP_NO_ERROR) || registry.Register(entry->label->registration) != CHIP_NO_ERROR) {
      labelsReady = false;
      break;
    }
  }
  chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  if (!labelsReady) {
    fail("Matter endpoint label registration failed; restart required");
    return false;
  }
  portENTER_CRITICAL(&_mux);
  _accepting = true;
  portEXIT_CRITICAL(&_mux);
  _state = connected() ? State::Connected : commissioned() ? State::Commissioned
                                                           : State::Commissioning;
  return true;
}

inline bool ArdMatter::publish(Entry& entry, const V& definition, const V& value) {
  if (!chip::DeviceLayer::PlatformMgr().TryLockChipStack()) return false;
    // Do not echo an older application value over a newer SDK command. The stack
    // lock keeps callbacks out between this check and the attribute update.
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  bool pending = false;
  portENTER_CRITICAL(&_mux);
  for (uint8_t i = 0; i < _queued; ++i)
    if (_commands[(_head + i) % 8].field == entry.field) pending = true;
  portEXIT_CRITICAL(&_mux);
  if (pending) {
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    return false;
  }
#endif
  _localReport = true;
  bool result = false;
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  // SDK startup restores persisted attributes after endpoint constructors have
  // filled their C++ caches. Refresh those caches before setters compare them
  // with application state, otherwise a matching stale cache can skip a report.
  auto sync = [&](uint32_t cluster, uint32_t attribute) {
    esp_matter_attr_val_t current{};
    return entry.endpoint->getAttributeVal(cluster, attribute, &current) && entry.endpoint->attributeChangeCB(entry.endpoint->getEndPointId(), cluster, attribute, &current);
  };
  bool synced = true;
  if (entry.kind == Kind::Outlet || entry.kind == Kind::OnOffLight || entry.kind == Kind::Dimmer || entry.kind == Kind::Light) synced = sync(6, 0);
  if (entry.kind == Kind::Dimmer || entry.kind == Kind::Light) synced &= sync(8, 0);
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
  if (entry.kind == Kind::Light) synced &= sync(0x300, 0) && sync(0x300, 1);
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
  if (entry.kind == Kind::Fan) synced &= sync(0x202, 0) && sync(0x202, 2);
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
  if (entry.kind == Kind::Climate) {
    synced &= sync(0x201, 0x1c) && sync(0x201, 0);
    if (ArdMatterFields::contains(definition["modes"], "heat") || ArdMatterFields::contains(definition["modes"], "heat_cool")) synced &= sync(0x201, 0x12);
    if (ArdMatterFields::contains(definition["modes"], "cool") || ArdMatterFields::contains(definition["modes"], "heat_cool")) synced &= sync(0x201, 0x11);
  }
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
  if (entry.kind == Kind::Cover) synced &= sync(0x102, 0xe) && sync(0x102, 0xf);
#endif
  if (!synced) {
    _localReport = false;
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    return false;
  }
#endif
#define ARD_MATTER_NATIVE(TYPE) static_cast<Native<TYPE>*>(entry.endpoint)
  switch (entry.kind) {
#if ARDPORTAL_ENABLE_CONTROL_SWITCH
    case Kind::Outlet: result = ARD_MATTER_NATIVE(MatterOnOffPlugin)->setOnOff(value.asBool()); break;
    case Kind::OnOffLight: result = ARD_MATTER_NATIVE(MatterOnOffLight)->setOnOff(value.asBool()); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_SLIDER
    case Kind::Dimmer:
      {
        auto* native = ARD_MATTER_NATIVE(MatterDimmablePlugin);
        const double number = value.asDouble();
        // Keep the remembered level while off. Do not round-trip a matching SDK
        // transition level through the coarser application slider step either.
        result = (number <= 0 || ArdMatterFields::fromLevel(native->getLevel(), definition) == number || native->setLevel(ArdMatterFields::level(number, definition["max"].asDouble()))) && native->setOnOff(number > 0);
        break;
      }
#endif
#if ARDPORTAL_ENABLE_CONTROL_TEXT
    case Kind::Temperature:
    case Kind::Humidity:
    case Kind::Pressure:
    case Kind::Illuminance:
      {
        double number = 0;
        bool known = !value.isNull() && value.toDouble(number) && std::isfinite(number);
        uint32_t cluster = entry.kind == Kind::Temperature ? 0x402 : entry.kind == Kind::Humidity ? 0x405
                                                                   : entry.kind == Kind::Pressure ? 0x403
                                                                                                  : 0x400;
        esp_matter_attr_val_t raw{};
        if (entry.kind == Kind::Temperature || entry.kind == Kind::Pressure) {
          double scaled = entry.kind == Kind::Temperature ? round(number * 100) : round(number);
          known &= scaled >= -32767 && scaled <= 32767 && (entry.kind != Kind::Temperature || number >= -273.15);
          raw = esp_matter_nullable_int16(known ? nullable<int16_t>(int16_t(scaled)) : nullable<int16_t>());
        } else {
          double scaled = entry.kind == Kind::Humidity ? round(number * 100) : number > 0 ? std::max(0.0, round(10000 * log10(number)) + 1)
                                                                                          : 0;
          known &= number >= 0 && (entry.kind != Kind::Humidity || number <= 100) && scaled <= 65534;
          raw = esp_matter_nullable_uint16(known ? nullable<uint16_t>(uint16_t(scaled)) : nullable<uint16_t>());
        }
        result = entry.endpoint->updateAttributeVal(cluster, 0, &raw);
        break;
      }
#endif
#if ARDPORTAL_ENABLE_CONTROL_BINARY_SENSOR
    case Kind::Contact: result = ARD_MATTER_NATIVE(MatterContactSensor)->setContact(!value.asBool()); break;
    case Kind::Occupancy: result = ARD_MATTER_NATIVE(MatterOccupancySensor)->setOccupancy(value.asBool()); break;
    case Kind::Leak: result = ARD_MATTER_NATIVE(MatterWaterLeakDetector)->setLeak(value.asBool()); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_LIGHT
    case Kind::Light:
      {
        auto* native = ARD_MATTER_NATIVE(MatterColorLight);
        espRgbColor_t rgb{ uint8_t(value["color"]["r"].asDouble()), uint8_t(value["color"]["g"].asDouble()), uint8_t(value["color"]["b"].asDouble()) };
        auto hsv = espRgbColorToHsvColor(rgb);
        hsv.v = std::max(uint8_t(1), ArdMatterFields::level(value["brightness"].asDouble(), 255));
        result = native->setColorHSV(hsv) && native->setOnOff(value["state"].asString() == ArdMatterFields::state(definition, true));
        break;
      }
#endif
#if ARDPORTAL_ENABLE_CONTROL_FAN
    case Kind::Fan: result = ARD_MATTER_NATIVE(MatterFan)->setSpeedPercent(uint8_t(value["percentage"].asDouble())) && ARD_MATTER_NATIVE(MatterFan)->setOnOff(value["state"].asString() == ArdMatterFields::state(definition, true)); break;
#endif
#if ARDPORTAL_ENABLE_CONTROL_CLIMATE
    case Kind::Climate:
      {
        auto* native = ARD_MATTER_NATIVE(MatterThermostat);
        String mode = value["mode"].asString();
        auto modeId = mode == "heat" ? MatterThermostat::THERMOSTAT_MODE_HEAT : mode == "cool"      ? MatterThermostat::THERMOSTAT_MODE_COOL
                                                                              : mode == "heat_cool" ? MatterThermostat::THERMOSTAT_MODE_AUTO
                                                                                                    : MatterThermostat::THERMOSTAT_MODE_OFF;
        result = native->setMode(modeId);
        if (ArdMatterFields::contains(definition["modes"], "heat_cool")) result &= native->setCoolingHeatingSetpoints(value["temperature_low"].asDouble(), value["temperature_high"].asDouble());
        else if (ArdMatterFields::contains(definition["modes"], "cool")) result &= native->setCoolingSetpoint(value["temperature"].asDouble());
        else if (ArdMatterFields::contains(definition["modes"], "heat")) result &= native->setHeatingSetpoint(value["temperature"].asDouble());
        auto local = value["current_temperature"];
        if (local.type() == V::Type::Number) result &= native->setLocalTemperature(local.asDouble());
        else {
          auto raw = esp_matter_nullable_int16(nullable<int16_t>());
          result &= native->updateAttributeVal(0x201, 0, &raw);
        }
        break;
      }
#endif
#if ARDPORTAL_ENABLE_CONTROL_COVER
    case Kind::Cover:
      {
        auto* native = ARD_MATTER_NATIVE(MatterWindowCovering);
        result = native->setCurrentLiftPercent100ths(uint16_t((100 - value["position"].asDouble()) * 100)) && native->setCurrentTiltPercent100ths(uint16_t((100 - value["tilt"].asDouble()) * 100));
        // Application state is authoritative: an attained position must not
        // leave the controller showing motion from an earlier target command.
        String state = value["state"].asString();
        result &= native->setOperationalStatus(state == "opening" ? 0x05 : state == "closing" ? 0x0a
                                                                                              : 0);
        break;
      }
#endif
    default: break;
  }
#undef ARD_MATTER_NATIVE
  _localReport = false;
  chip::DeviceLayer::PlatformMgr().UnlockChipStack();
  return result;
}
