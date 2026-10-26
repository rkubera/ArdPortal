// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_MATTER
#include "MatterFields.h"
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
#include "MatterDiagnostics.h"
namespace chip {
namespace DeviceLayer {
struct ChipDeviceEvent;
}
}
#endif
#include <Matter.h>
#ifndef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL
#error Select a Matter-enabled ESP32 core/board profile
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class ArdPortal;
class ArdMatterLabel;
// One SDK node per device. Use a global ArdPortal; endpoints live until reboot.
class ArdMatter {
public:
  enum class State : uint8_t { Waiting,
                               Staging,
                               Commissioning,
                               Commissioned,
                               Connected,
                               Failed,
                               Resetting,
                               NoEndpoints };
  struct Options {
#if !ARDPORTAL_MATTER_PRODUCTION
    uint32_t passcode = 20202021;  // Development override; production uses factory provisioning.
    uint16_t discriminator = 3840;
#endif
    bool bleCommissioning = true;
  };
  /** @brief Create an idle adapter; ArdPortal owns startup and endpoint lifetime. */
  ArdMatter() = default;
  /** @brief Detach callbacks and endpoint ownership; SDK endpoint storage remains alive until restart. */
  ~ArdMatter();
  /** @brief Prevent copying the adapter and its native endpoint ownership. */
  ArdMatter(const ArdMatter&) = delete;
  /** @brief Prevent assigning native endpoint ownership from another adapter. */
  ArdMatter& operator=(const ArdMatter&) = delete;
  /** @brief Read the loop-owned lifecycle state, including informational NoEndpoints. */
  State state() const {
    return _state;
  }
  /** @brief Return whether the native SDK has successfully started. */
  bool started() const {
    return _running;
  }
  /** @brief Return whether endpoint registration is frozen; Waiting and NoEndpoints allow additions. */
  bool frozen() const {
    return _state != State::Waiting && _state != State::NoEndpoints;
  }
  /** @brief Return whether the running SDK has a commissioned fabric; cached by loop(). */
  bool commissioned() const;
  /** @brief Return whether commissioned Matter has transport connectivity; not a controller-session check. */
  bool connected() const;
  /** @brief Return the number of staged supported application endpoints. */
  uint8_t endpointCount() const {
    return _count;
  }
  /** @brief Return the number of fields skipped because no compatible Matter mapping exists. */
  uint16_t unsupportedCount() const {
    return _unsupported;
  }
  /** @brief Return borrowed startup error text; empty when no failure is recorded. */
  const char* error() const {
    return _error;
  }
  /** @brief Return the cached development manual code after startup; empty in production or before startup. */
  String manualPairingCode() const;
  /** @brief Return the SDK development onboarding URL after startup; empty in production or before startup. */
  String onboardingUrl() const;
  /** @brief Open a basic pairing window for 180–900 seconds. Return false if stopped, resetting, locked or rejected by the SDK. */
  bool openCommissioningWindow(uint16_t seconds = 300);
  /** @brief Queue SDK fabric/network reset and restart, preserving ArdFS/factory credentials. Return false if stopped, already queued or NVS update fails. */
  bool reset();  // SDK fabric/network reset and restart; preserves ArdFS.
  /** @brief Copy startup options before inventory freeze. Return false when frozen or discriminator exceeds 4095; production uses factory identity. */
  bool configure(const Options& options);

private:
  friend class ArdPortal;
  friend class ArdAppControls;
  using V = ArdJSON::JSONVar;
  using Kind = ArdMatterFields::Kind;
  using Operation = ArdMatterFields::Operation;
  using Command = ArdMatterFields::Command;
  struct Entry {
    ArdMatter* owner;
    Entry* next = nullptr;
    MatterEndPoint* endpoint = nullptr;
    uint16_t field;
    Kind kind;
    bool dirty = true;
    ArdMatterLabel* label = nullptr;
  };
  /** @brief Decode a native attribute write into an application operation and compact payload. */
  int prepareCommand(uint32_t cluster, uint32_t attribute, const esp_matter_attr_val_t* value, Operation& operation, uint32_t& raw);
  template<class T> struct Native;
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  ArdMatterDiagnostics _diagnostics;
  bool _diagnosticRegistered = false;
  /** @brief Queue bounded SDK lifecycle events for later loop-owned logging. */
  static void diagnosticEvent(const chip::DeviceLayer::ChipDeviceEvent* event, intptr_t context);
#endif
  /** @brief Refresh cached fabric/transport/discovery state at most once per second using nonblocking SDK locks. */
  void refreshConnectivity();
  Options _options;
  Entry* _entries = nullptr;
  Entry* _report = nullptr;
  State _state = State::Waiting;
  const char* _error = "";
  size_t _build = 0, _seen = 0;
  uint32_t _networkAt = 0;
  uint32_t _stableSince = 0, _reportAt = 0, _layout = 2166136261UL;
  uint16_t _unsupported = 0;
  uint8_t _count = 0;
  bool _commissioned = false, _connected = false;
#if ARDPORTAL_ENABLE_WWW
  bool _commissioningInProgress = false;
#endif
#if ARDPORTAL_MATTER_THREAD
  bool _threadProvisioned = false, _wifiConnected = false;
#if ARDPORTAL_ENABLE_WWW
  uint8_t _threadRole = 0, _threadSrpHost = 7, _threadDiscovery = 0;
#endif
#endif
  bool _selected = false, _running = false, _localReport = false, _accepting = false, _reset = false;
  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
#if ARDPORTAL_MATTER_COMMAND_SUPPORT
  Command _commands[8]{};
  uint8_t _head = 0, _queued = 0;
#endif
#if ARDPORTAL_MATTER_LOG_SUPPORT && ARDPORTAL_MATTER_COMMAND_SUPPORT
  uint8_t _commandDrops = 0;
#endif
  /** @brief Queue a native attribute command without JSON work; return false on queue saturation. */
  bool enqueue(Entry& entry, Operation operation, uint32_t value);
  /** @brief Remove the oldest queued command; return false when no command is pending. */
  bool pop(Command& command);
  /** @brief Discard queued commands superseded by a newer application intent for this field. */
  void discard(uint16_t field);
  /** @brief Schedule coalesced reporting for the matching application field. */
  void dirty(uint16_t field);
  /** @brief Stage a supported native endpoint; return false on allocation or SDK creation failure. */
  bool add(uint16_t field, const V& definition, const V& value);
  /** @brief Validate credentials/layout and start the SDK; return false for empty inventory or startup failure. */
  bool start(const char* name, const char* manufacturer);
  /** @brief Report application state under the CHIP lock; return false to retry after contention or SDK failure. */
  bool publish(Entry& entry, const V& definition, const V& value);
  /** @brief Apply SDK-coordinated saving admission; return whether the running transport can save now. */
  bool powerSaving(bool selected, bool busy);
#if defined(CONFIG_ENABLE_ICD_SERVER) && CONFIG_ENABLE_ICD_SERVER
  uint32_t _powerActiveAt = 0;
#endif
  /** @brief Store borrowed static error text and enter Failed state. */
  void fail(const char* message) {
    _error = message;
    _state = State::Failed;
  }
};
#endif
