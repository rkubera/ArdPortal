// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "LanguageLiterals.h"
#include "ArdZigbee.h"
#include "ZigbeeConsolePayload.h"
#include "ConsoleTimestamp.h"
#include "ZigbeeTime.h"
#include <sys/time.h>
#include <esp_partition.h>
#include <ha/esp_zigbee_ha_standard.h>
#include <zdo/esp_zigbee_zdo_common.h>
#include <nwk/esp_zigbee_nwk.h>
#if !ARDPORTAL_ZIGBEE_ROUTER
extern "C" {
#include <zboss_api.h>
  // Same startup descriptor workaround used by Arduino-ESP32 ZigbeeCore.
  void zb_set_ed_node_descriptor(bool power_src, bool rx_on_when_idle, bool alloc_addr);
}
#endif

#if ARDPORTAL_ENABLE_WIFI && CONFIG_ESP_COEX_SW_COEXIST_ENABLE && CONFIG_SOC_IEEE802154_SUPPORTED
#include <esp_coexist.h>
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "ZigbeeNativeImpl.h"
#endif
ArdZigbee* ArdZigbee::_instance = nullptr;
/** @brief Deliver native Zigbee signals to the portal module. @param signal SDK signal. @return No value. */
void esp_zb_app_signal_handler(esp_zb_app_signal_t* signal) {
  ArdZigbee::signal(signal);
}
/** @brief Allocate queues and start asynchronous native Zigbee initialization. @param options Operation limits. @return True when accepted; false on allocation or partition error. */
bool ArdZigbee::begin(const Options& options) {
  if (!validOptions(options)) {
    _error = ARD_LANG_NATIVE_INVALID_ZIGBEE_TIMEOUT_RETRY_LIMITS;
    setState(State::Failed);
    return false;
  }
  _options = options;
  if (_begun) return true;
  if (_instance && _instance != this) {
    _error = ARD_LANG_NATIVE_ONLY_ONE_ZIGBEE_STACK_IS_SUPPORTED;
    return false;
  }
  if (!esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "zb_storage") || !esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "zb_fct")) {
    _error = ARD_LANG_NATIVE_SELECT_A_ZIGBEE_PARTITION_SCHEME_ZB_STORAGE_AND_ZB_FCT_R;
    setState(State::Failed);
    return false;
  }
  _events = xQueueCreate(16, sizeof(Event));
  if (!_events) {
    _error = ARD_LANG_NATIVE_CANNOT_ALLOCATE_ZIGBEE_EVENT_QUEUE;
    setState(State::Failed);
    return false;
  }
  _traffic = xQueueCreate(16, 128);
  if (!_traffic) {
    vQueueDelete(_events);
    _events = nullptr;
    _error = ARD_LANG_NATIVE_CANNOT_ALLOCATE_ZIGBEE_TRAFFIC_QUEUE;
    setState(State::Failed);
    return false;
  }
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (_controlCount) {
    _commands = xQueueCreate(16, sizeof(ArdZigbeeFields::Command));
    if (!_commands) {
      vQueueDelete(_events);
      vQueueDelete(_traffic);
      _events = nullptr;
      _traffic = nullptr;
      _error = ARD_LANG_NATIVE_CANNOT_ALLOCATE_ZIGBEE_COMMAND_QUEUE;
      setState(State::Failed);
      return false;
    }
  }
#endif
  _instance = this;
  _begun = true;
  _operation = Operation::Pair;
  _since = millis();
  _retryAt = _since;
  if (xTaskCreate(task, "ArdZigbee", 6144, this, 5, nullptr) != pdPASS) {
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
    if (_commands) {
      vQueueDelete(_commands);
      _commands = nullptr;
    }
#endif
    _instance = nullptr;
    _begun = false;
    vQueueDelete(_events);
    vQueueDelete(_traffic);
    _events = nullptr;
    _traffic = nullptr;
    _operation = Operation::None;
    _error = ARD_LANG_NATIVE_CANNOT_ALLOCATE_ZIGBEE_TASK;
    setState(State::Failed);
    return false;
  }
  log(ARD_LANG_NATIVE_ZIGBEE_STACK_STARTING_AUTOMATIC_PAIR_REJOIN_ENABLED);
  return true;
}
/** @brief Initialize the native stack and run its scheduler. @param arg Owning module. @return No value. */
void ArdZigbee::task(void* arg) {
  auto& self = *static_cast<ArdZigbee*>(arg);
  esp_zb_platform_config_t platform = {};
  platform.radio_config.radio_mode = ZB_RADIO_MODE_NATIVE;
  platform.host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;
  if (esp_zb_platform_config(&platform) != ESP_OK) {
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
  esp_zb_cfg_t config = {};
  config.esp_zb_role = ARDPORTAL_ZIGBEE_ROUTER ? ESP_ZB_DEVICE_TYPE_ROUTER : ESP_ZB_DEVICE_TYPE_ED;
  config.install_code_policy = false;
#if ARDPORTAL_ZIGBEE_ROUTER
  config.nwk_cfg.zczr_cfg.max_children = 10;
#else
  config.nwk_cfg.zed_cfg.ed_timeout = ESP_ZB_ED_AGING_TIMEOUT_64MIN;
  config.nwk_cfg.zed_cfg.keep_alive = 3000;
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (!ArdHeap::permits(8192 + size_t(self._controlCount) * 512, 4096)) {
    self.traffic(ARD_LANG_NATIVE_INSUFFICIENT_HEAP8_DMA_RESERVE_FOR_NATIVE_ZIGBEE_ENDPOIN);
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  // Each reportable attribute can retain both an interval timer and pending
  // reporting work. Keep the SDK's default capacity for network commissioning.
  uint16_t schedulerSlots = 80;
  for (auto* entry = self._controls.get(); entry; entry = entry->next.get()) schedulerSlots += uint16_t(entry->initialCount) * 2;
  if (esp_zb_scheduler_queue_size_set(schedulerSlots) != ESP_OK) {
    snprintf(self._startupError, sizeof(self._startupError), ARD_LANG_NATIVE_CANNOT_SIZE_ZIGBEE_REPORTING_SCHEDULER);
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
#endif
#if !ARDPORTAL_ZIGBEE_ROUTER
  // The SDK ignores RxOnWhenIdle changes after joining. Select the network
  // capability before initialization/start, including saved-network rejoin.
  esp_zb_sleep_enable(self._sleepyProfile);
#endif
  esp_zb_init(&config);
#if !ARDPORTAL_ZIGBEE_ROUTER
  esp_zb_set_rx_on_when_idle(!self._sleepyProfile);
  self.traffic(self._sleepyProfile ? ARD_LANG_NATIVE_ZIGBEE_SLEEPY_END_DEVICE_PROFILE_SELECTED : ARD_LANG_NATIVE_ZIGBEE_END_DEVICE_RECEIVER_ENABLED_WHILE_IDLE);
#endif
#if ARDPORTAL_ENABLE_WIFI && CONFIG_ESP_COEX_SW_COEXIST_ENABLE && CONFIG_SOC_IEEE802154_SUPPORTED
  if (esp_coex_wifi_i154_enable() != ESP_OK) {
    snprintf(self._startupError, sizeof(self._startupError), ARD_LANG_NATIVE_COULD_NOT_ENABLE_WI_FI_ZIGBEE_RADIO_COEXISTENCE);
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
  self.traffic(ARD_LANG_NATIVE_WI_FI_IEEE_802_15_4_RADIO_COEXISTENCE_ENABLED);
#endif
  esp_zb_aps_data_indication_handler_register(received);
  esp_zb_configuration_tool_cfg_t epConfig = ESP_ZB_DEFAULT_CONFIGURATION_TOOL_CONFIG();
  auto* clusters = esp_zb_configuration_tool_clusters_create(&epConfig);
  auto* basic = clusters ? esp_zb_cluster_list_get_cluster(clusters, ESP_ZB_ZCL_CLUSTER_ID_BASIC, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE) : nullptr;
  static char manufacturer[] = "\x09"
                               "ArdPortal";
  auto* model = ArdZigbeeIdentity::Model;
  if (!basic || esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufacturer) != ESP_OK || esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model) != ESP_OK || esp_zb_basic_cluster_add_attr(basic, 0x4000, const_cast<char*>(self._firmwareVersion)) != ESP_OK) {
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
  auto* timeClient = esp_zb_zcl_attr_list_create(0x000a);
  if (!timeClient || esp_zb_cluster_list_add_time_cluster(clusters, timeClient, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE) != ESP_OK) {
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
  auto* endpoints = esp_zb_ep_list_create();
  esp_zb_endpoint_config_t endpoint = {};
  endpoint.endpoint = 1;
  endpoint.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  endpoint.app_device_id = ESP_ZB_HA_CONFIGURATION_TOOL_DEVICE_ID;
  if (!endpoints || esp_zb_ep_list_add_ep(endpoints, clusters, endpoint) != ESP_OK) {
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (!self.createControls(endpoints)) {
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
  if (!ArdHeap::permits(4096 + size_t(self._controlCount) * 128, 1024)) {
    self.traffic(ARD_LANG_NATIVE_INSUFFICIENT_NETWORK_RESERVE_BEFORE_ZIGBEE_DEVICE_REGIST);
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
  esp_zb_core_action_handler_register(action);
#endif
  if (!endpoints || esp_zb_device_register(endpoints) != ESP_OK) {
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
  esp_zb_set_primary_network_channel_set(0x07FFF800);
  if (esp_zb_start(false) != ESP_OK) {
    self.event(4, ESP_FAIL);
    vTaskDelete(nullptr);
    return;
  }
#if !ARDPORTAL_ZIGBEE_ROUTER
  // start(false) restores NVRAM before BDB/network startup. Its saved joined
  // flag makes the esp_zb_* receiver setter a no-op, even here. Reapply the
  // staged capability through ZBOSS before the scheduler can rejoin; update
  // the advertised descriptor as Arduino's sleepy-device workaround does.
  zb_set_rx_on_when_idle(self._sleepyProfile ? ZB_FALSE : ZB_TRUE);
  zb_set_ed_node_descriptor(true, !self._sleepyProfile, true);
#endif
  esp_zb_stack_main_loop();
  vTaskDelete(nullptr);
}
/** @brief Copy a lifecycle result to the bounded loop queue. @param type Result kind. @param status SDK status. @return No value. */
void ArdZigbee::event(uint8_t type, int32_t status) {
  Event e;
  e.type = type;
  e.status = status;
  if (type != 4) e.factoryNew = esp_zb_bdb_is_factory_new();
  if (type != 4 && status == ESP_OK) {
    e.channel = esp_zb_get_current_channel();
    e.pan = esp_zb_get_pan_id();
  }
  // Bounded native events; overwrite the oldest record if the consumer falls behind.
  if (xQueueSend(_events, &e, 0) != pdTRUE) {
    Event discarded;
    xQueueReceive(_events, &discarded, 0);
    xQueueSend(_events, &e, 0);
  }
}
// Network data is persisted by the SDK's native NVRAM backend. No leave,
// factory reset or unconditional dataset rewrite belongs on the sleep path.
bool ArdZigbee::canDeepSleep() const {
#if ARDPORTAL_ZIGBEE_ROUTER
  return false;
#else
  if (!_ready || busy() || _state != State::Connected || uint32_t(millis() - _powerConnectedAt) < 5000) return false;
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  if (otaActive()) return false;
#endif
  if (!_powerSaving.load() || !_sleepyProfile || !_deepSleepWanted.load()) return false;
  const uint32_t idle = _powerIdleAt.load();
  if (!idle || uint32_t(millis() - (idle - 1)) > 100) return false;
  if ((_events && uxQueueMessagesWaiting(_events)) || _timeSequence.load() < 256) return false;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (_commands && uxQueueMessagesWaiting(_commands)) return false;
  for (auto* entry = _controls.get(); entry; entry = entry->next.get())
    if (entry->reportQueued || entry->pendingReports) return false;
#endif
  return true;
#endif
}
bool ArdZigbee::prepareDeepSleep() {
  if (!canDeepSleep() || !esp_zb_lock_acquire(0)) return false;
  // Hold the native lock through Wi-Fi shutdown and sleep entry. Native flash
  // writes run synchronously under this lock; no stack work can start midway.
  if (!canDeepSleep()) {
    esp_zb_lock_release();
    return false;
  }
  _deepSleepLocked = true;
  return true;
}
void ArdZigbee::cancelDeepSleep() {
  if (_deepSleepLocked) {
    _deepSleepLocked = false;
    esp_zb_lock_release();
  }
}
bool ArdZigbee::powerSaving(bool enable, bool deepSleep) {
#if ARDPORTAL_ZIGBEE_ROUTER
  (void)enable;
  (void)deepSleep;
  return false;
#else
  _deepSleepWanted.store(deepSleep);
  if (!_begun) {
    _sleepyProfile = enable;
    return false;
  }
  if (!_ready) return false;
  enable = enable && !busy() && _state == State::Connected;
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  enable = enable && !otaActive();
#endif
  // Enabling sleep on an already joined full-receiver device needs a restart;
  // never claim success for the SDK's silent no-op setter in that state.
  if (enable && !_sleepyProfile) return false;
  if (!_powerConfigured || enable != _powerSaving.load()) {
    if (!esp_zb_lock_acquire(0)) return false;
    const bool applied = enable && !esp_zb_get_rx_on_when_idle();
    esp_zb_sleep_enable(applied);
    _powerSaving.store(applied);
    _powerConfigured = true;
    esp_zb_lock_release();
    return applied;
  }
  return enable && _powerSaving.load();
#endif
}
/** @brief Observe SDK signals and schedule only initial stack commissioning. @param signal Borrowed SDK signal. @return No value. */
void ArdZigbee::signal(esp_zb_app_signal_t* signal) {
  if (!_instance || !signal || !signal->p_app_signal) return;
  auto type = static_cast<esp_zb_app_signal_type_t>(*signal->p_app_signal);
  auto status = signal->esp_err_status;
#if !ARDPORTAL_ZIGBEE_ROUTER
  // This callback runs inside the native scheduler. Never force light-sleep
  // from Arduino loop beneath the stack. The SDK suspends its own radio/task;
  // IDF may sleep the CPU only when every task and PM lock permits it.
  if (type == ESP_ZB_COMMON_SIGNAL_CAN_SLEEP) {
    _instance->_powerIdleAt.store(status == ESP_OK ? millis() + 1 : 0);
    if (status == ESP_OK) {
      if (_instance->_powerSaving.load() && !_instance->_deepSleepWanted.load()) esp_zb_sleep_now();
    }
    return;  // Sleep readiness is frequent; do not fill the diagnostic queue.
  }
#endif
  _instance->_powerIdleAt.store(0);
  char message[128];
  snprintf(message, sizeof(message), ARD_LANG_NATIVE_SIGNAL_S_STATUS_0X_LX, esp_zb_zdo_signal_to_string(type), (unsigned long)status);
  _instance->traffic(message);
  switch (type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      if (esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION) != ESP_OK) _instance->event(4, ESP_FAIL);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT: _instance->event(1, status); break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
    case ESP_ZB_BDB_SIGNAL_TC_REJOIN_DONE: _instance->event(2, status); break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE: _instance->event(3, status); break;
    case ESP_ZB_NWK_SIGNAL_NO_ACTIVE_LINKS_LEFT: _instance->event(6, status); break;
    default: break;
  }
}
/** @brief Notify a real state transition from loop. @param state New state. @return No value. */
void ArdZigbee::setState(State state) {
  if (state == State::Failed && _ready && _autoConnect) {
    if (!_reconnectPending) {
      _reconnectPending = true;
      _reconnectAt = millis() + 30000;
    }
  } else _reconnectPending = false;
  if (_state == state) return;
  if (state == State::Connected) _powerConnectedAt = millis();
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (state == State::Connected)
    for (auto* entry = _controls.get(); entry; entry = entry->next.get()) entry->pendingReports = 0;
#endif
  _state = state;
  if (_changed) _changed(state);
}
/** @brief Append a bounded Zigbee-only history record. @param text Diagnostic text. @return No value. */
void ArdZigbee::log(const String& text, bool lifecycle) {
  if (lifecycle && _diagnosticSink) _diagnosticSink(_diagnosticContext, text);
  const char* data = text.c_str();
  if (data == _history[_lineId % HistoryCapacity].text.c_str()) {
    String copy = text;
    logText(copy.c_str(), copy.length());
  } else logText(data, text.length());
}
/** @brief Format a log record without temporary Strings or copying a traffic message.
 * @param text Borrowed diagnostic bytes. @param bytes Diagnostic length. @return No value.
 */
void ArdZigbee::logText(const char* text, size_t bytes) {
  bytes = std::min(bytes, size_t(220));
  char timestamp[32];
  const time_t current = time(nullptr);
  if (current < 1704067200 || !ArdConsoleTime::utcTimestamp(current, timestamp, sizeof(timestamp))) snprintf(timestamp, sizeof(timestamp), ARD_LANG_NATIVE_LU_MS, (unsigned long)millis());
  auto& line = _history[_lineId % HistoryCapacity];
  const size_t timestampBytes = strlen(timestamp);
  if (!line.text.reserve(timestampBytes + bytes + 3)) return;
  line.text = "[";
  line.text.concat(timestamp, timestampBytes);
  line.text += "] ";
  line.text.concat(text, bytes);
  line.id = ++_lineId;
#if ARDPORTAL_ZIGBEE_DEBUG_SERIAL
  // Serial belongs to the sketch. Never wait for a host or a full transmit buffer.
  const size_t outputBytes = line.text.length();
  if (Serial.availableForWrite() >= int(outputBytes + 1)) {
    Serial.write(reinterpret_cast<const uint8_t*>(line.text.c_str()), outputBytes);
    const uint8_t newline = '\n';
    Serial.write(&newline, 1);
  }
#endif
}
/** @brief Find oldest available record newer than cursor. @param cursor Last sent record ID. @return Borrowed record or nullptr. */
const ArdZigbee::Line* ArdZigbee::nextLine(uint32_t cursor) const {
  const Line* next = nullptr;
  for (const auto& line : _history)
    if (line.id > cursor && (!next || line.id < next->id)) next = &line;
  return next;
}
/** @brief Complete an accepted operation once. @param ok Success. @param reason Result detail. @param stopped User cancellation flag. @return No value. */
void ArdZigbee::finish(bool ok, const char* reason, bool stopped) {
  String result = reason;
  auto op = _operation;
  _operation = Operation::None;
  _waiting = false;
  _resetPending = false;
  _cancelRequested = false;
  _resetCancelPending = false;
  _resetWaitingCancel = false;
  _error = reason;
  log(ok ? (op == Operation::Rejoin ? ARD_LANG_NATIVE_ZIGBEE_REJOINED_SAVED_NETWORK : op == Operation::FactoryReset ? ARD_LANG_NATIVE_ZIGBEE_NETWORK_RESET_COMPLETED
                                                                                                                    : ARD_LANG_NATIVE_ZIGBEE_PAIRED_AND_CONNECTED)
         : ARD_LANG_NATIVE_ZIGBEE_OPERATION_FAILED + _error);
  setState(ok ? (op == Operation::FactoryReset ? State::Idle : State::Connected) : (stopped ? State::Idle : State::Failed));
  auto cb = _finished;
  if (op != Operation::None && cb) cb(op, ok, result);
}
/** @brief Queue an operation and establish its total deadline. @param op Operation. @param options Retry limits. @return True on acceptance. */
bool ArdZigbee::request(Operation op, const Options& options) {
  if (op == Operation::FactoryReset && busy()) {
    _resetCancelPending = _waiting;
    _cancelledOperation = _operation;
    _operation = Operation::None;
    _waiting = false;
  }
  if (!_begun || (!_ready && _state == State::Failed) || busy() || op == Operation::None || !validOptions(options)) return false;
  if (op == Operation::Rejoin && (!_ready || !_known)) {
    _error = ARD_LANG_NATIVE_NO_SAVED_ZIGBEE_NETWORK_USE_PAIR;
    return false;
  }
  if (op == Operation::Pair && _known) {
    _error = ARD_LANG_NATIVE_ALREADY_PAIRED_USE_REJOIN_OR_RESET_PAIRING;
    return false;
  }
  _reconnectPending = false;
  _autoConnect = op != Operation::FactoryReset;
  _options = options;
  _operation = op;
  _since = millis();
  _retryAt = _since;
  _attempts = 0;
  _waiting = false;
  _cancelRequested = false;
  _resetPending = op == Operation::ResetPairing || op == Operation::FactoryReset;
  _error = "";
  setState(_resetPending ? State::Resetting : op == Operation::Rejoin ? State::Rejoining
                                                                      : State::Pairing);
  log(op == Operation::Pair ? ARD_LANG_NATIVE_PAIRING_REQUESTED_ENABLE_JOINING_ON_COORDINATOR : op == Operation::Rejoin ? ARD_LANG_NATIVE_REJOIN_REQUESTED_PRESERVING_ZIGBEE_IDENTITY
                                                                                                                        : ARD_LANG_NATIVE_RESET_PAIRING_REQUESTED_WI_FI_AND_APPCONFIG_ARE_PRESERVE);
  return true;
}
/** @brief Drain native results and advance bounded retry work. @param now Milliseconds. @return No value. */
void ArdZigbee::loop(uint32_t now) {
  if (!_begun) return;
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  // The SDK task can refresh activity after `now` was sampled by the caller.
  // A slightly newer activity tick is not an elapsed timeout, including wrap.
  if (_otaActive && int32_t(now - _otaLastActivity.load()) >= 60000 && esp_zb_lock_acquire(0)) {
    if (_otaHandle) {
      esp_ota_abort(_otaHandle);
      _otaHandle = 0;
    }
    _otaActive = false;
    esp_zb_lock_release();
    log(ARD_LANG_NATIVE_ZIGBEE_OTA_TIMED_OUT_CURRENT_FIRMWARE_RETAINED);
  }
#endif
  if (_cancelledOperation != Operation::None) {
    auto op = _cancelledOperation;
    _cancelledOperation = Operation::None;
    auto cb = _finished;
    if (cb) cb(op, false, String(F(ARD_LANG_NATIVE_CANCELLED_BY_FACTORY_RESET)));
  }
  Event e;
  for (unsigned budget = 0; budget < 8 && xQueueReceive(_events, &e, 0) == pdTRUE; ++budget) {

    if (e.type == 2 && _resetWaitingCancel) {
      _resetWaitingCancel = false;
      _retryAt = now;
    }
    if (e.type == 4) {
      _ready = false;
      finish(false, _startupError[0] ? _startupError : ARD_LANG_NATIVE_STACK_INITIALIZATION_FAILED);
      continue;
    }
    if (e.type == 1) {
      _ready = true;
      _known = !e.factoryNew;
      if (!_bootPairHandled) {
        _bootPairHandled = true;
        if (_known && _operation == Operation::Pair) _operation = Operation::Rejoin;
      }
      if (!_known && !busy()) setState(State::Idle);
      if (_state == State::Starting && busy()) setState(_operation == Operation::Rejoin ? State::Rejoining : State::Pairing);
    }
    if (e.type == 6) {
      setState(State::Failed);
      log(ARD_LANG_NATIVE_ZIGBEE_LINK_LOST);
    }
    if ((e.type == 1 || e.type == 2) && e.status == ESP_OK && !e.factoryNew) {
      _known = true;
      _channel = e.channel;
      _pan = e.pan;
      if (busy() && !_resetPending) finish(true, "");
      else if (!busy()) {
        const bool restored = _state != State::Connected;
        setState(State::Connected);
        if (restored) log(ARD_LANG_NATIVE_ZIGBEE_CONNECTED_TO_SAVED_NETWORK);
      }
    } else if ((e.type == 1 || e.type == 2) && e.status != ESP_OK) {
      _ready = true;
      _waiting = false;
      _retryAt = now + _options.retryMs;
      if (!busy()) setState(State::Failed);
    }
    if (e.type == 3 && e.status == ESP_OK) {
      _known = !e.factoryNew;
      _channel = 0;
      _pan = 0;
      _waiting = false;
      if (_resetPending && e.factoryNew) {
        _resetPending = false;
        if (_operation == Operation::FactoryReset) finish(true, "");
        else {
          _retryAt = now;
          setState(State::Pairing);
        }
      } else if (!busy()) setState(State::Idle);
    }
  }
  char message[128];
  for (unsigned budget = 0; budget < 4 && xQueueReceive(_traffic, message, 0) == pdTRUE; ++budget) logText(message, strlen(message));
  if (!busy() && _reconnectPending && _ready && _autoConnect && int32_t(now - _reconnectAt) >= 0) {
    log(_known ? ARD_LANG_NATIVE_AUTOMATIC_REJOIN_PRESERVING_SAVED_ZIGBEE_NETWORK : ARD_LANG_NATIVE_AUTOMATIC_PAIRING_RETRY_ENABLE_JOINING_ON_COORDINATOR);
    request(_known ? Operation::Rejoin : Operation::Pair, _options);
  }
  serviceTime(now);
  if (!busy()) return;
  if (_cancelRequested && _ready) {
    if (!esp_zb_lock_acquire(0)) return;
    auto result = esp_zb_bdb_cancel_steering();
    esp_zb_lock_release();
    if (result != ESP_OK) {
      finish(false, ARD_LANG_NATIVE_COULD_NOT_STOP_PAIRING);
      return;
    }
    finish(false, ARD_LANG_NATIVE_PAIRING_STOPPED, true);
    return;
  }
  if (int32_t(now - _since) >= int32_t(_options.timeoutMs)) {
    if (_ready && esp_zb_lock_acquire(0)) {
      esp_zb_bdb_cancel_steering();
      esp_zb_lock_release();
    }
    finish(false, ARD_LANG_NATIVE_OPERATION_TIMED_OUT);
    return;
  }
  if (!_ready) return;
  if (_resetCancelPending) {
    if (!esp_zb_lock_acquire(0)) return;
    auto result = _known ? ESP_FAIL : esp_zb_bdb_cancel_steering();
    esp_zb_lock_release();
    _resetCancelPending = false;
    if (result == ESP_OK) {
      _resetWaitingCancel = true;
      log(ARD_LANG_NATIVE_CANCELLING_ACTIVE_STEERING_BEFORE_FACTORY_RESET);
      return;
    }
  }
  if (_resetWaitingCancel) return;
  if (_waiting) {
    if (int32_t(now - _attemptSince) < int32_t(_options.attemptTimeoutMs)) return;
    if (!esp_zb_lock_acquire(0)) return;
    esp_zb_bdb_cancel_steering();
    esp_zb_lock_release();
    _waiting = false;
    _retryAt = now + _options.retryMs;
    log(ARD_LANG_NATIVE_ATTEMPT_TIMED_OUT_RETRY_SCHEDULED);
    return;
  }
  if (int32_t(now - _retryAt) < 0) return;
  if (_attempts >= _options.maxAttempts) {
    finish(false, ARD_LANG_NATIVE_RETRY_LIMIT_REACHED);
    return;
  }
  if (!esp_zb_lock_acquire(0)) return;
  if (_resetPending && _known) {
    esp_zb_bdb_reset_via_local_action();
    _waiting = true;
    _attemptSince = now;
    esp_zb_lock_release();
    return;
  }
  _resetPending = false;
  if (_operation == Operation::FactoryReset) {
    esp_zb_lock_release();
    finish(true, "");
    return;
  }
  auto result = esp_zb_bdb_start_top_level_commissioning(_operation == Operation::Rejoin ? ESP_ZB_BDB_MODE_INITIALIZATION : ESP_ZB_BDB_MODE_NETWORK_STEERING);
  esp_zb_lock_release();
  ++_attempts;
  _attemptSince = now;
  _waiting = result == ESP_OK;
  _retryAt = now + _options.retryMs;
  log(ARD_LANG_NATIVE_TX_COMMISSIONING_ATTEMPT + String(_attempts) + (result == ESP_OK ? ARD_LANG_NATIVE_STARTED : ARD_LANG_NATIVE_COULD_NOT_START));
}

/** @brief Queue stack diagnostics without dynamic allocation. @param text Formatted message. @return No value. */
void ArdZigbee::traffic(const char* text) {
  if (!_traffic) return;
  char message[128];
  snprintf(message, sizeof(message), "%s", text);
  if (xQueueSend(_traffic, message, 0) != pdTRUE) {
    char discarded[128];
    xQueueReceive(_traffic, discarded, 0);
    xQueueSend(_traffic, message, 0);
  }
}
/** @brief Observe APS metadata without taking over stack processing. @param frame Borrowed APS indication. @return False so the stack continues processing it. */
bool ArdZigbee::received(esp_zb_apsde_data_ind_t frame) {
  if (_instance) {
    _instance->_powerIdleAt.store(0);
    char metadata[128];
    snprintf(metadata, sizeof(metadata), ARD_LANG_NATIVE_RX_FRAME, unsigned(frame.src_short_addr), unsigned(frame.src_endpoint), unsigned(frame.dst_endpoint), unsigned(frame.profile_id), unsigned(frame.cluster_id), frame.lqi, unsigned(frame.status));
    _instance->traffic(metadata);
    if (frame.profile_id != 0x104) {
      _instance->traffic(ARD_LANG_NATIVE_RX_UNSUPPORTED_PROFILE);
      return false;
    }
    if (frame.cluster_id == 0x000a && frame.src_short_addr == 0 && frame.src_endpoint == 1 && frame.dst_endpoint == 1 && frame.status == 0) {
      uint32_t utc;
      uint16_t sequence = _instance->_timeSequence.load();
      if (sequence < 256 && ArdZigbeeTime::decode(static_cast<const uint8_t*>(frame.asdu), frame.asdu_length, uint8_t(sequence), utc)) _instance->_timeReceived.store(utc);
    }
    ArdZigbeeConsole::decode(static_cast<const uint8_t*>(frame.asdu), frame.asdu_length, frame.cluster_id, [&](const char* payload, uint8_t sequence) {
      char message[128];
      snprintf(message, sizeof(message), ARD_LANG_NATIVE_RX_EP_U_SEQ_U_PAYLOAD_S, unsigned(frame.dst_endpoint), sequence, payload);
      _instance->traffic(message);
    });
  }
  return false;
}

/** @brief Request cooperative cancellation of pairing and retries. @return True if accepted. */
bool ArdZigbee::stopPairing() {
  if (!busy() && _reconnectPending && !_known) {
    _autoConnect = false;
    _reconnectPending = false;
    setState(State::Idle);
    return true;
  }
  if (!busy() || (_operation != Operation::Pair && _operation != Operation::ResetPairing) || _resetPending) return false;
  _autoConnect = false;
  _reconnectPending = false;
  _cancelRequested = true;
  return true;
}

/** @brief Query the coordinator clock and apply a verified response in loop context. @param now Milliseconds. @return No value. */
void ArdZigbee::serviceTime(uint32_t now) {
  bool connected = _state == State::Connected;
  if (!connected) {
    _timeConnected = false;
    _timeSequence = 256;
    _timeReceived = 0;
    return;
  }
  if (!_timeConnected) {
    _timeConnected = true;
    _timeProbeAt = now + 1000;
  }
  uint32_t utc = _timeReceived.exchange(0);
  if (utc) {
    _timeSequence = 256;
    timeval clock = {};
    clock.tv_sec = utc;
    if (settimeofday(&clock, nullptr) == 0) {
      log(ARD_LANG_NATIVE_ZIGBEE_TIME_SYNCHRONIZED_FROM_COORDINATOR);
      _timeProbeAt = now + 21600000UL;
    } else {
      log(ARD_LANG_NATIVE_ZIGBEE_TIME_COULD_NOT_SET_SYSTEM_CLOCK);
      _timeProbeAt = now + 300000UL;
    }
  }
  if (_timeSequence.load() < 256) {
    if (int32_t(now - _timeProbeAt) < 0) return;
    _timeSequence = 256;
    _timeProbeAt = now + 3600000UL;
    log(ARD_LANG_NATIVE_ZIGBEE_TIME_COORDINATOR_DID_NOT_PROVIDE_A_VALID_TIME_RES);
    return;
  }
  if (int32_t(now - _timeProbeAt) < 0 || !esp_zb_lock_acquire(0)) return;
  uint16_t attributes[] = { 0, 1 };
  esp_zb_zcl_read_attr_cmd_t request = {};
  request.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  request.zcl_basic_cmd.dst_addr_u.addr_short = 0;
  request.zcl_basic_cmd.dst_endpoint = 1;
  request.zcl_basic_cmd.src_endpoint = 1;
  request.clusterID = 0x000a;
  request.attr_number = 2;
  request.attr_field = attributes;
  _timeSequence = esp_zb_zcl_read_attr_cmd_req(&request);
  esp_zb_lock_release();
  _timeProbeAt = now + 10000UL;
  log(ARD_LANG_NATIVE_ZIGBEE_TIME_PROBING_COORDINATOR_TIME_CLUSTER);
}
