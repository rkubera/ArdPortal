// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include <functional>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_zigbee_core.h>
#include <aps/esp_zigbee_aps.h>
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "ZigbeeFields.h"
#include <memory>
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
#include <atomic>
#include <esp_ota_ops.h>
#include "ZigbeeOtaImage.h"
#endif
#endif

// Model identity must agree on the management and application endpoints.
namespace ArdZigbeeIdentity {
#if ARDPORTAL_ZIGBEE_LAYOUT_SUPPORT
static constexpr size_t ModelCapacity = 40;
#else
static constexpr size_t ModelCapacity = 14;
#endif
#if defined(CONFIG_IDF_TARGET_ESP32C5)
static char Model[ModelCapacity] = "\x0c"
                                   "ArdPortal-C5";
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
static char Model[ModelCapacity] = "\x0c"
                                   "ArdPortal-H2";
#else
static char Model[ModelCapacity] = "\x0c"
                                   "ArdPortal-C6";
#endif
}

// All application callbacks and String allocations run in the Arduino loop.
class ArdZigbee {
public:
  /** Stage the sleepy profile before begin(); later toggle native sleep only.
   * RxOnWhenIdle is fixed at network startup. Routers always refuse sleep. */
  bool powerSaving(bool enable, bool deepSleep = false);
  bool canDeepSleep() const;
  bool prepareDeepSleep();
  void cancelDeepSleep();
  enum class Operation : uint8_t { None,
                                   Pair,
                                   Rejoin,
                                   ResetPairing,
                                   FactoryReset };
  enum class State : uint8_t { Starting,
                               Idle,
                               Pairing,
                               Rejoining,
                               Resetting,
                               Connected,
                               Failed };
  struct Options {
    uint32_t timeoutMs = 60000, retryMs = 3000, attemptTimeoutMs = 15000;
    uint8_t maxAttempts = 3;
  };
  using FinishedCallback = std::function<void(Operation, bool, const String&)>;
  using StateCallback = std::function<void(State)>;
  struct Line {
    uint32_t id = 0;
    String text;
  };
  static constexpr size_t HistoryCapacity = 16;
  /** @brief Validate operation limits. @param options Limits to inspect. @return True for positive timings up to 600000 ms and 1-20 attempts. */
  static bool validOptions(const Options& options) {
    return options.timeoutMs && options.timeoutMs <= 600000 && options.retryMs && options.retryMs <= 600000 && options.attemptTimeoutMs && options.attemptTimeoutMs <= 600000 && options.maxAttempts && options.maxAttempts <= 20;
  }
  /** @brief Schedule stack initialization in a separate task. @return True if accepted. */
  bool begin(const Options& options);
  /** @brief Borrow stable application version metadata before startup. @param version ZCL length-prefixed SWBuildID owned by the portal. @return No value. */
  void setFirmwareVersion(const char* version) {
    if (!_begun && version) _firmwareVersion = version;
  }

  /** @brief Advance queued signals, retries and callbacks. @param now Milliseconds. @return No value. */
  void loop(uint32_t now);
  /** @brief Request a network operation. @param op Pair, rejoin or reset. @param options Timeout/retry limits. @return True if accepted. */
  bool request(Operation op, const Options& options);
  /** @brief Request cancellation of active pairing. @return True if cancellation was accepted. */
  bool stopPairing();
  /** @brief Install operation completion callback. @param cb Handler, called from loop. @return No value. */
  void onFinished(FinishedCallback cb) {
    _finished = cb;
  }
  /** @brief Install state callback. @param cb Handler, called from loop. @return No value. */
  void onState(StateCallback cb) {
    _changed = cb;
  }
  /** @brief Install an independent lifecycle diagnostic sink. @param context Borrowed owner. @param sink Loop-owned diagnostic handler; nullptr disables forwarding. @return No value. */
  void onLifecycleDiagnostic(void* context, void (*sink)(void*, const String&)) {
    _diagnosticContext = context;
    _diagnosticSink = sink;
  }
  /** @brief Get current state. @return Cached loop-owned state. */
  State state() const {
    return _state;
  }
  /** @brief Enable or suspend automatic pairing/rejoin retries. @param enabled Retry permission. @return No value; an active operation continues until stopped. */
  void setAutoConnect(bool enabled) {
    _autoConnect = enabled;
    if (enabled && _state != State::Connected) {
      _reconnectPending = true;
      _reconnectAt = millis();
    } else if (!enabled) _reconnectPending = false;
  }
  /** @brief Read automatic retry policy. @return True when enabled. */
  bool autoConnect() const {
    return _autoConnect;
  }
  /** @brief Check operation in progress. @return True while busy. */
  bool busy() const {
    return _operation != Operation::None;
  }
  /** @brief Check pairing including the pause between automatic retry series. @return True while pairing is active or scheduled. */
  bool pairing() const {
    return (busy() && _state == State::Pairing) || (_reconnectPending && !_known && _autoConnect);
  }
  /** @brief Check saved network identity. @return True when known. */
  bool hasNetwork() const {
    return _known;
  }
  /** @brief Get network channel. @return Cached channel, or zero. */
  uint8_t channel() const {
    return _channel;
  }
  /** @brief Check whether the native stack finished initialization. @return True when SDK queries are safe. */
  bool nativeReady() const {
    return _ready;
  }
  /** @brief Get PAN identifier. @return Cached PAN ID. */
  uint16_t pan() const {
    return _pan;
  }
  /** @brief Get current attempt count. @return Attempts started. */
  uint8_t attempts() const {
    return _attempts;
  }
  /** @brief Get latest operation error. @return Borrowed error string. */
  const String& error() const {
    return _error;
  }
  /** @brief Find next console record. @param cursor Last delivered ID. @return Borrowed record or nullptr. */
  const Line* nextLine(uint32_t cursor) const;
  /** @brief Append a loop-owned diagnostic to the Zigbee console. @param text Message. @return No value. */
  void diagnostic(const String& text) {
    log(text, false);
  }
  /** @brief Handle SDK signal without application callbacks. @param signal SDK signal. @return No value. */
  static void signal(esp_zb_app_signal_t* signal);
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  struct FreeAttributes {
    /** @brief Release staged attributes. @param pointer Allocated memory. @return No value. */
    void operator()(ArdZigbeeFields::Attribute* pointer) const {
      std::free(pointer);
    }
  };
  struct FreeLabel {
    /** @brief Release the startup-only ZCL label. @param pointer Allocated bytes. @return No value. */
    void operator()(char* pointer) const {
      std::free(pointer);
    }
  };
  struct Control {
    ArdZigbeeFields::Spec spec;
    std::unique_ptr<ArdZigbeeFields::Attribute, FreeAttributes> initial;
    std::unique_ptr<char, FreeLabel> label;
    std::unique_ptr<Control> next;
    uint32_t reportRetryAt = 0;
    uint16_t pendingReports = 0;
    uint8_t initialCount = 0;
    bool reportQueued : 1;
    bool reportForce : 1;
    bool reportRetry : 1;
    bool reportIncomplete : 1;
    /** @brief Initialize compact report flags. @return No value. */
    Control()
      : reportQueued(false), reportForce(false), reportRetry(false), reportIncomplete(false) {}
    /** @brief Allocate a node without throwing. @param bytes Size. @return Memory or nullptr. */
    static void* operator new(size_t bytes, const std::nothrow_t&) noexcept {
      return std::malloc(bytes);
    }
    /** @brief Release a node. @param pointer Memory. @return No value. */
    static void operator delete(void* pointer) noexcept {
      std::free(pointer);
    }
    /** @brief Release a node after construction failure. @param pointer Memory. @return No value. */
    static void operator delete(void* pointer, const std::nothrow_t&) noexcept {
      std::free(pointer);
    }
  };
  /** @brief Stage one endpoint before begin. @param spec Mapping. @param initial Initial attributes. @param label Field label. @return False on memory, endpoint limit or late registration. */
  bool addControl(ArdZigbeeFields::Spec spec, const ArdZigbeeFields::Values& initial, const String& label);
  /** @brief Read immutable endpoint mappings. @return Borrowed list. */
  const Control* controls() const {
    return _controls.get();
  }
  /** @brief Queue reports for one field, or all fields. @param field Field index; size_t(-1) selects all. @param force Also report unchanged values. @return No value. */
  void queueReports(size_t field, bool force = false) {
    for (auto* entry = _controls.get(); entry; entry = entry->next.get())
      if (field == size_t(-1) || entry->spec.field == field) {
        entry->reportQueued = true;
        if (force) {
          entry->reportForce = true;
          entry->reportRetry = false;
          entry->pendingReports = 0;
        }
      }
  }
  /** @brief Select the next ready endpoint fairly. @param now Current time. @param after Previous endpoint cursor. @return Borrowed queued endpoint or nullptr. */
  const Control* nextReport(uint32_t now, uint8_t after) const {
    const Control* wrap = nullptr;
    for (auto* entry = _controls.get(); entry; entry = entry->next.get())
      if (entry->reportQueued && (!entry->reportRetry || int32_t(now - entry->reportRetryAt) >= 0)) {
        if (entry->spec.endpoint > after) return entry;
        if (!wrap) wrap = entry;
      }
    return wrap;
  }
  /** @brief Complete one report attempt without blocking other endpoints. @param endpoint Attempted endpoint. @param success All attributes queued successfully. @param now Current time. @return No value. */
  void finishReport(uint8_t endpoint, bool success, uint32_t now) {
    for (auto* entry = _controls.get(); entry; entry = entry->next.get())
      if (entry->spec.endpoint == endpoint) {
        if (success) {
          entry->reportQueued = false;
          entry->reportForce = false;
          entry->reportRetry = false;
        } else if (entry->reportIncomplete) {
          entry->reportRetry = false;
        } else {
          entry->reportRetry = true;
          entry->reportRetryAt = now + 5000;
        }
        return;
      }
  }
  /** @brief Get endpoint count. @return Number of staged application endpoints. */
  uint8_t controlCount() const {
    return _controlCount;
  }
  /** @brief Dequeue a native command in loop. @param command Destination. @return True if a command was available. */
  bool nextCommand(ArdZigbeeFields::Command& command) {
    return _commands && xQueueReceive(_commands, &command, 0) == pdTRUE;
  }
  /** @brief Update and report attributes while retaining network reserve. @param endpoint Destination. @param values Desired state. @param force Report even unchanged values after join or a command. @return True if all writes/reports were accepted. */
  bool publish(uint8_t endpoint, const ArdZigbeeFields::Values& values, bool force = false);
#endif
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  /** @brief Enable safe OTA admission from the portal loop. @param allowed Portal operations permit OTA. @return No value. */
  void allowOta(bool allowed) {
    _otaAllowed = allowed;
  }
  /** @brief Check whether native firmware reception is active. @return True during OTA. */
  bool otaActive() const {
    return _otaActive.load() || _otaFinished.load();
  }
  /** @brief Consume successful firmware completion once. @return True when the portal must schedule its restart sequence. */
  bool takeOtaFinished() {
    return _otaFinished.exchange(false);
  }
  /** @brief Submit a standard OTA query. @return True when submitted. */
  bool queryOta();
#endif
private:
  std::atomic<bool> _powerSaving{ false }, _deepSleepWanted{ false };
  std::atomic<uint32_t> _powerIdleAt{ 0 };
  uint32_t _powerConnectedAt = 0;
  bool _deepSleepLocked = false, _sleepyProfile = false, _powerConfigured = false;
  const char* _firmwareVersion = "\x03"
                                 "1.0";
  std::atomic<uint32_t> _timeReceived{ 0 };
  std::atomic<uint16_t> _timeSequence{ 256 };
  uint32_t _timeProbeAt = 0;
  bool _timeConnected = false;
  /** @brief Probe coordinator Time support and apply verified UTC responses without blocking. @param now Milliseconds. @return No value. */
  void serviceTime(uint32_t now);
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  std::atomic<bool> _otaAllowed{ false }, _otaActive{ false }, _otaFinished{ false };
  std::atomic<uint32_t> _otaLastActivity{ 0 };
  esp_ota_handle_t _otaHandle = 0;
  const esp_partition_t* _otaPartition = nullptr;
  ArdZigbeeOtaImage _otaImage;
  uint8_t _otaEndpoint = 0;
  bool _otaVerified = false;
  /** @brief Validate and write native OTA messages. @param message SDK object. @return Native result. */
  esp_err_t otaAction(const esp_zb_zcl_ota_upgrade_value_message_t* message);
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  std::unique_ptr<Control> _controls;
  uint8_t _controlCount = 0, _reportSequence = 0;
  QueueHandle_t _commands = nullptr;
  /** @brief Build staged endpoints before stack start. @param endpoints Native list. @return True if all clusters were added. */
  bool createControls(esp_zb_ep_list_t* endpoints);
  /** @brief Copy a supported SDK callback into the loop queue. @param id Callback kind. @param message Borrowed message. @return SDK result. */
  static esp_err_t action(esp_zb_core_action_callback_id_t id, const void* message);
#endif
  struct Event {
    uint8_t type = 0, channel = 0;
    uint16_t pan = 0;
    int32_t status = 0;
    bool factoryNew = true;
  };
  QueueHandle_t _events = nullptr, _traffic = nullptr;
  State _state = State::Starting;
  Operation _operation = Operation::None, _cancelledOperation = Operation::None;
  Options _options;
  bool _begun = false, _ready = false, _known = false, _waiting = false, _resetPending = false, _bootPairHandled = false, _cancelRequested = false, _resetCancelPending = false, _resetWaitingCancel = false;
  uint8_t _channel = 0, _attempts = 0;
  uint16_t _pan = 0;
  uint32_t _since = 0, _attemptSince = 0, _retryAt = 0, _lineId = 0, _reconnectAt = 0;
  bool _reconnectPending = false, _autoConnect = true;
  String _error;
  char _startupError[96] = {};  // SDK task publishes this before its failure event.
  Line _history[HistoryCapacity];
  FinishedCallback _finished;
  StateCallback _changed;
  void* _diagnosticContext = nullptr;
  void (*_diagnosticSink)(void*, const String&) = nullptr;
  static ArdZigbee* _instance;
  /** @brief Observe an incoming APS frame without consuming it. @param frame SDK frame. @return False to preserve normal stack processing. */
  static bool received(esp_zb_apsde_data_ind_t frame);
  /** @brief Queue fixed-size traffic diagnostics. @param text Formatted SDK message. @return No value. */
  void traffic(const char* text);
  /** @brief Run initialization and Zigbee scheduler. @param arg Instance. @return No value. */
  static void task(void* arg);
  /** @brief Queue stack event. @param type Event kind. @param status SDK result. @return No value. */
  void event(uint8_t type, int32_t status);
  /** @brief Change cached state. @param state New state. @return No value. */
  void setState(State state);
  /** @brief Finish accepted request once. @param ok Result. @param reason Error or empty. @return No value. */
  void finish(bool ok, const char* reason, bool stopped = false);
  /** @brief Append bounded diagnostic record. @param text Message. @return No value. */
  void log(const String& text, bool lifecycle = true);
  /** @brief Write a diagnostic directly to a reusable history buffer. @param text Borrowed bytes. @param bytes Text length. @return No value. */
  void logText(const char* text, size_t bytes);
};
