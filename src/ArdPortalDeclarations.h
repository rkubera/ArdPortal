// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#include "HeapMemory.h"
#include "ArdAsset.h"
#include <Arduino.h>
namespace ArdUILanguageData { enum class Key : uint16_t; }
#include <functional>
#include <memory>
#include <time.h>
#include <DNSServer.h>
#include "ArdFS.h"
#include "ArdJSON.h"
#include "JsonCodec.h"
#include "AppConfigRegistrationError.h"
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "DynamicPages.h"
#include "AppConfigPageRegistration.h"
#endif
#include "PortalTypes.h"
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "ArdAppControls.h"
#endif
#if ARDPORTAL_ENABLE_MQTT
#include "ArdMqtt.h"
#endif
#if ARDPORTAL_ENABLE_HA
#include "ArdHomeAssistant.h"
#endif
#if ARDPORTAL_ENABLE_OTA
#include "ArdOta.h"
#endif
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#error ArdPortal requires an ESP8266 or ESP32 with Wi-Fi and an Arduino core
#endif

// Single-threaded: call begin(), loop() and public methods from the Arduino task.
class ArdPortal {
public:
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: ArdPortalBuildTag<ARDPORTAL_FEATURE_MASK>.
   * @return No value.
   */
  ArdPortal(ArdPortalBuildTag<ARDPORTAL_FEATURE_MASK> = {});
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: const ArdPortal&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdPortal(const ArdPortal&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: const ArdPortal&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdPortal& operator=(const ArdPortal&) = delete;
  /**
   * @brief Disallow copying or moving this resource-owning instance.
   * Input: ArdPortal&&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdPortal(ArdPortal&&) = delete;
  /**
   * @brief Replace this instance state with the supplied value.
   * Input: ArdPortal&&.
   * @return No result; this operation is deleted and cannot be called.
   */
  ArdPortal& operator=(ArdPortal&&) = delete;
  using PortalConfig = ArdPortalConfig;
  static constexpr size_t MaxPortalConfigBytes = 5 * 1024;
  static constexpr size_t MaxAppConfigBytes = 8 * 1024;
  static constexpr size_t MaxConfigDocumentBytes = 13 * 1024 + 64;
  struct Options {
    const char* deviceName = nullptr; // Default: ArdUI- + chip ID; also the AP SSID.
    const char* apPassword = "1234567890";
    const char* ntpServer1 = "pool.ntp.org";
    const char* ntpServer2 = "time.cloudflare.com";
    uint32_t appStateIntervalMs = 600000; // Retained application state refresh.
    uint32_t discoveryIntervalMs = 0; // Retained Home Assistant discovery refresh.
    uint32_t haWorkBudgetMs = 2; // Cooperative budget between HA work units.
    uint32_t appConfigRegistrationWorkBudgetMs = 2; // Cooperative budget between registration units.
    uint16_t appConfigRegistrationMaxOperationsPerLoop = 16;
    uint16_t haMaxOperationsPerLoop = 1; // Return control between HA work units.
    uint32_t appConfigSaveDelayMs = 750; // Coalesce application changes.
    uint32_t appConfigMinWriteIntervalMs = 5000;
    uint32_t wifiTimeoutMs = 30000;
    uint32_t retryMs = 10000;
    uint32_t mqttTimeoutMs = 15000;
    uint32_t tcpTimeoutMs = 250; // Synchronous connect; DNS has a separate core timeout.
    uint16_t keepAliveSeconds = 30;
    uint16_t tlsHandshakeTimeoutSeconds = 5; // ESP32 only; ESP8266 uses BearSSL core defaults.
  };
  enum class WifiState { NoCredentials, Connecting, Connected, FallbackAP };
  using MqttState = ArdMqttState;
  using ChangeSource = ArdPortalChangeSource;
  using RestartReason = ArdPortalRestartReason;
  using RestartCallback = std::function<void(RestartReason)>;
  using PortalConfigChangedCallback = std::function<void(const PortalConfig&, ChangeSource)>;
  using PortalAndAppConfigSavedCallback = std::function<void(bool, const String&)>;
  using PortalAndAppConfigReadyCallback = std::function<void(bool)>;
  using MessageCallback = std::function<void(const String&, const uint8_t*, size_t)>;
  /**
   * @brief Install the callback notified when portal settings change.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onPortalConfigChanged(PortalConfigChangedCallback callback) { _changed = callback; }
  /**
   * @brief Install the callback run before manual or OTA restarts; factory reset and storage formatting bypass it.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onBeforeRestart(RestartCallback callback) { _beforeRestart = callback; }
  /**
   * @brief Install the callback reporting completion of a persistent configuration save.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onPortalAndAppConfigSaved(PortalAndAppConfigSavedCallback callback) { _saved = callback; }
  /**
   * @brief Install the callback reporting completion of initial configuration loading.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onPortalAndAppConfigReady(PortalAndAppConfigReadyCallback callback) { _readyCallback = callback; }
  /**
   * @brief Check whether initial portal and application configuration loading has completed.
   * @return True once initial configuration loading has completed, including a failed load.
   */
  bool portalAndAppConfigReady() const { return _configurationReady; }
  /**
   * @brief Check for active storage work, a pending save or a pending ready notification.
   * @return True if storage work, a scheduled save or a ready notification is pending.
   */
  bool portalAndAppConfigBusy() const { return _storage.busy() || _savePending || _pendingReady; }
  // true = accepted in RAM; onPortalAndAppConfigSaved signals durable completion.
  /**
   * @brief Validate and apply portal settings in RAM, then schedule persistent storage.
   * @param config Portal settings to validate and apply.
   * @return True if accepted in RAM; persistent completion is reported through onPortalAndAppConfigSaved().
   */
  bool setPortalConfig(const PortalConfig& config);
  /**
   * @brief Set portal config device manufacturer.
   * @param manufacturer Manufacturer text stored in the portal device metadata.
   * @return True if the metadata update was accepted and scheduled for persistence; false if rejected.
   */
  bool setPortalConfigDeviceManufacturer(const String& manufacturer);
  /**
   * @brief Set portal config device description.
   * @param description Device description stored in portal metadata.
   * @return True if the metadata update was accepted and scheduled for persistence; false if rejected.
   */
  bool setPortalConfigDeviceDescription(const String& description);
  /**
   * @brief Read a custom application configuration value by key.
   * @param key Configuration key or JSON object member name.
   * @return The stored application value, or Undefined when the key is missing.
   */
  ArdJSON::JSONVar getAppConfigValue(const char* key) const;
  /**
   * @brief Update a custom application configuration value and schedule its persistence.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True if accepted in RAM; persistent completion is reported through onPortalAndAppConfigSaved().
   */
  bool setAppConfigValue(const char* key, const ArdJSON::JSONVar& value);
  /**
   * @brief Remove a custom application configuration value and schedule persistence.
   * @param key Configuration key or JSON object member name.
   * @return True if accepted in RAM; persistent completion is reported through onPortalAndAppConfigSaved().
   */
  bool removeAppConfigValue(const char* key);
  /**
   * @brief Expose the stage, field identifier and cause of the latest registration error.
   * @return Reference to the last registration error, including stage, fieldId and reason.
   */
  const ArdAppConfigRegistrationError& appConfigRegistrationError() const { return _appConfigRegistrationError; }
  enum class AppConfigPageRegistrationState { Idle, Pending, Succeeded, Failed };
  using AppConfigPageRegistrationFinishedCallback=std::function<void(bool)>;
  /**
   * @brief Install the completion callback for accepted AppConfig page registration jobs.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onAppConfigPageRegistrationFinished(AppConfigPageRegistrationFinishedCallback callback) { _appConfigPageRegistrationFinished=callback; }
  /**
   * @brief Read the current page registration job state.
   * @return Idle, Pending, Succeeded or Failed for the current or most recent registration.
   */
  AppConfigPageRegistrationState appConfigPageRegistrationState() const {return _appConfigPageRegistrationState;}
  /**
   * @brief Check whether a page registration job is pending.
   * @return True while registration state is Pending.
   */
  bool appConfigPageRegistrationBusy() const {return _appConfigPageRegistrationState==AppConfigPageRegistrationState::Pending;}
  /**
   * @brief Read how many fields the current registration job has processed.
   * @return Number of fields already processed by the registration job.
   */
  size_t appConfigPageRegistrationProcessedFields() const {return _appConfigPageRegistrationProcessed;}
  /**
   * @brief Read the total field count discovered for the current registration job.
   * @return Number of fields discovered so far in the registration source.
   */
  size_t appConfigPageRegistrationTotalFields() const {return _appConfigPageRegistrationTotal;}
  // All accepted jobs finish through loop(); small pages take one pass.
  /**
   * @brief Accept an AppConfig page registration job; complete accepted jobs from loop(), including small pages.
   * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
   * @return True if accepted; loop() later delivers the completion callback. False rejects the job without a callback.
   */
  bool startAppConfigPageRegistration(const String& definition);
  /**
   * @brief Accept an AppConfig page registration job; complete accepted jobs from loop(), including small pages.
   * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
   * @return True if accepted; loop() later delivers the completion callback. False rejects the job without a callback.
   */
  bool startAppConfigPageRegistration(String&& definition); // Transfers the source on acceptance.
  /**
   * @brief Accept an AppConfig page registration job; complete accepted jobs from loop(), including small pages.
   * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
   * @return True if accepted; loop() later delivers the completion callback. False rejects the job without a callback.
   */
  bool startAppConfigPageRegistration(const __FlashStringHelper* definition); // Immutable source must outlive the portal.
  /**
   * @brief Register a Home Assistant entity without adding a page to the portal navigation.
   * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool addAppConfigEntity(const String& definition);
  /**
   * @brief Register a Home Assistant entity without adding a page to the portal navigation.
   * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool addAppConfigEntity(const __FlashStringHelper* definition);
  using AppConfigCommandCallback = std::function<bool(const String&, const String&, const ArdJSON::JSONVar&, ChangeSource)>;
  /**
   * @brief Install the handler for application action commands from the portal or MQTT.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onAppConfigCommand(AppConfigCommandCallback callback) {
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
    _appCommand=callback;
#else
    (void)callback;
#endif
  }
  /**
   * @brief Update a runtime application value and optionally queue its MQTT state.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @param publishMqtt Whether to queue MQTT state publication for this update.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool setAppConfigStateValue(const char* key, const ArdJSON::JSONVar& value, bool publishMqtt = true);
  /**
   * @brief Mark an application field for MQTT state publication.
   * @param key Configuration key or JSON object member name.
   * @return True when the field is found and queued; false for an unknown or unsupported field.
   */
  bool queueAppConfigStatePublish(const char* key);
  /**
   * @brief Emit a transient application event through the field MQTT topic.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool emitAppConfigEvent(const char* key, const ArdJSON::JSONVar& value);
  using AppConfigValueChangedCallback = std::function<void(const String&, const ArdJSON::JSONVar&, ChangeSource)>;
  /**
   * @brief Install the callback notified when an application configuration value changes.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onAppConfigValueChanged(AppConfigValueChangedCallback callback) { _appChanged = callback; }
  /**
   * @brief Request a configuration save without debounce; flash work continues through loop().
   * @return True if accepted in RAM; persistent completion is reported through onPortalAndAppConfigSaved().
   */
  bool flushPortalAndAppConfig(); // Skip debounce, never perform flash I/O here.
  /**
   * @brief Append a diagnostic message to Console Messages when console messages are enabled.
   * @param message Diagnostic text to append.
   * @return No value.
   */
  void log(const String& message); // Diagnostic messages in Console > Messages.
  /**
   * @brief Append a diagnostic message to Console Messages when console messages are enabled.
   * @param message Diagnostic text to append.
   * @return No value.
   */
  void logMessage(const String& message) { log(message); } // Explicit diagnostic logging; no Serial interception.
  /**
   * @brief Expose the last portal/application configuration storage error.
   * @return Reference to the latest storage error text; empty when no error is recorded.
   */
  const String& portalAndAppConfigError() const { return _storageError; }
  /**
   * @brief Build a stable hexadecimal identifier from the device hardware.
   * @return Stable hardware identifier encoded in hexadecimal.
   */
  static String chipId();
  /**
   * @brief Build the default device name from the ArdUI prefix and hardware identifier.
   * @return ArdUI- followed by the hexadecimal hardware identifier.
   */
  static String defaultDeviceName() { return "ArdUI-" + chipId(); }
  /**
   * @brief Install the callback for MQTT messages not handled by application controls.
   * @param callback Completion or event handler supplied to this operation.
   * @return No value.
   */
  void onMqttMessage(MessageCallback callback) {
#if ARDPORTAL_ENABLE_MQTT
    _mqttClient.onMessage(std::move(callback));
#else
    (void)callback;
#endif
  }
#if ARDPORTAL_ENABLE_MQTT
  /**
   * @brief Expose the MQTT component owned by this portal.
   * @return Reference to the portal MQTT component.
   */
  ArdMqtt& mqtt() { return _mqttClient; }
  /**
   * @brief Expose the MQTT component owned by this portal.
   * @return Reference to the portal MQTT component.
   */
  const ArdMqtt& mqtt() const { return _mqttClient; }
#endif
#if ARDPORTAL_ENABLE_HA
  /**
   * @brief Expose the Home Assistant discovery component owned by this portal.
   * @return Reference to the portal Home Assistant component.
   */
  ArdHomeAssistant& homeAssistant() { return _homeAssistant; }
  /**
   * @brief Expose the Home Assistant discovery component owned by this portal.
   * @return Reference to the portal Home Assistant component.
   */
  const ArdHomeAssistant& homeAssistant() const { return _homeAssistant; }
#endif
#if ARDPORTAL_ENABLE_OTA
  /**
   * @brief Expose the OTA component owned by this portal.
   * @return Reference to the portal OTA component.
   */
  ArdOta& ota() { return _ota; }
  /**
   * @brief Expose the OTA component owned by this portal.
   * @return Reference to the portal OTA component.
   */
  const ArdOta& ota() const { return _ota; }
#endif
  /**
   * @brief Initialize networking, storage and portal services using default or supplied options.
   * @return True when startup is accepted; initial storage completion is reported separately by the ready callback.
   */
  bool begin();
  /**
   * @brief Initialize networking, storage and portal services using default or supplied options.
   * @param options Initialization options and cooperative work limits.
   * @return True when startup is accepted; initial storage completion is reported separately by the ready callback.
   */
  bool begin(const Options& options);
  /**
   * @brief Advance the component work; call repeatedly from the Arduino main loop.
   * @return No value.
   */
  void loop();
  /**
   * @brief Check the Arduino Wi-Fi station connection status.
   * @return True when WiFi.status() is WL_CONNECTED.
   */
  bool wifiConnected() const { return WiFi.status() == WL_CONNECTED; }
  /**
   * @brief Check whether the portal MQTT session is connected.
   * @return True when the MQTT protocol session is connected.
   */
  bool mqttConnected() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.connected();
#else
    return false;
#endif
  }
  /**
   * @brief Read the portal Wi-Fi connection state.
   * @return The current portal Wi-Fi state.
   */
  WifiState wifiState() const { return _wifiState; }
  /**
   * @brief Read the MQTT connection state.
   * @return The current MQTT state.
   */
  MqttState mqttState() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.state();
#else
    return MqttState::Disabled;
#endif
  }
  /**
   * @brief Check whether the configuration access point is active.
   * @return True when the configuration AP is active.
   */
  bool apActive() const { return _apActive; }
  /**
   * @brief Check whether the clock is valid enough for TLS certificate verification.
   * @return True when system time is valid for certificate checks.
   */
  bool tlsClockReady() const { return time(nullptr) >= 1704067200; } // 2024-01-01
  /**
   * @brief Check portal and app config storage ok.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool portalAndAppConfigStorageOK() const { return _storageOK; }
  /**
   * @brief Read the Wi-Fi station IP address.
   * @return The Wi-Fi station IP address.
   */
  IPAddress localIP() const { return WiFi.localIP(); }
  /**
   * @brief Read the configuration access point IP address.
   * @return The AP IP address.
   */
  IPAddress apIP() const { return WiFi.softAPIP(); }
  /**
   * @brief Expose the current portal configuration.
   * @return Reference to the current portal settings.
   */
  const PortalConfig& getPortalConfig() const { return _config; }
  // QoS 0; false means disconnected, invalid arguments or a busy send buffer.
  /**
   * @brief Build an MQTT topic from its prefix, configured device name and suffix.
   * @param kind Topic prefix such as cmnd or stat.
   * @param command Command or state suffix appended to the topic.
   * @return The assembled MQTT topic.
   */
  String mqttTopic(const char* kind, const char* command) const;
  /**
   * @brief Validate an MQTT topic or subscription filter, including wildcard placement.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param subscription Whether wildcard subscription-filter rules are allowed.
   * @return True for a supported topic/filter; false for invalid characters, length or wildcard use.
   */
  bool validMqttTopic(const String& topic, bool subscription = false) const;
  /**
   * @brief Queue an MQTT message for cooperative transmission.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param payload Message bytes or text to send or decode.
   * @param retain Whether the broker should retain this MQTT message.
   * @return True if queued; false if disconnected, the topic is invalid or the packet cannot fit.
   */
  bool publish(const char* topic, const char* payload, bool retain = false);
  /**
   * @brief Queue an MQTT subscription request.
   * @param topic MQTT topic to publish, subscribe or match.
   * @return True if queued; false if disconnected or the topic/filter or packet is invalid.
   */
  bool subscribe(const char* topic);
  /**
   * @brief Validate the device name, Wi-Fi, MQTT and TLS portal settings.
   * @param config Portal settings to validate and apply.
   * @return True if all portal settings pass validation; false with reason set otherwise.
   */
  static bool validPortalConfig(const PortalConfig& config);
private:
  ArdAppConfigRegistrationError _appConfigRegistrationError;
  AppConfigPageRegistrationFinishedCallback _appConfigPageRegistrationFinished;
  AppConfigPageRegistrationState _appConfigPageRegistrationState=AppConfigPageRegistrationState::Idle;
  size_t _appConfigPageRegistrationProcessed=0,_appConfigPageRegistrationTotal=0;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  std::unique_ptr<ArdAppConfigPageRegistration> _appConfigPageRegistration;
  /**
   * @brief Prepare the source and staging state for an atomic page registration.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @param flash Immutable definition in program memory; must outlive the portal.
   * @param owned Optional RAM string whose ownership is transferred on acceptance.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool beginAppConfigPageRegistration(const String* source,const __FlashStringHelper* flash,String* owned=nullptr);
  /**
   * @brief Advance page registration within the configured cooperative work budget.
   * @return No value.
   */
  void serviceAppConfigPageRegistration();
  /**
   * @brief Perform one bounded unit of page registration work.
   * @return No value.
   */
  void serviceAppConfigPageRegistrationUnit();
  bool _appConfigPageRegistrationDriving=false;
  uint32_t _appConfigPageRegistrationGeneration=0;
  /**
   * @brief Commit the completed page and arrange delivery of its completion callback.
   * @param success Whether registration completed successfully.
   * @return No value.
   */
  void finishAppConfigPageRegistration(bool success);
  /**
   * @brief Record a registration failure and discard its uncommitted staging state.
   * @param stage Registration stage recorded in the error report.
   * @param field Application field definition or identifier.
   * @param reason Restart or failure reason.
   * @return No value.
   */
  __attribute__((noinline)) void failAppConfigPageRegistration(const char* stage,const String& field,const __FlashStringHelper* reason);
#endif
  /**
   * @brief Check app config registration failed.
   * @param stage Registration stage recorded in the error report.
   * @param field Application field definition or identifier.
   * @param reason Restart or failure reason.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool appConfigRegistrationFailed(const char* stage,const String& field,const String& reason) {
    _appConfigRegistrationError={stage,field,reason};
    log(String("AppConfig registration: stage=")+stage+" field="+field+" reason="+reason);return false;
  }
  friend class ArdMqtt;
  friend class ArdHomeAssistant;
  friend class ArdOta;
  friend class ArdAppControls;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  ArdAppControls _appControls;
#endif
  /**
   * @brief Validate and apply an incoming application configuration change.
   * @param app Application configuration object.
   * @return No value.
   */
  void applyAppConfig(ArdJSON::JSONVar app);
#if ARDPORTAL_ENABLE_MQTT
  ArdMqtt _mqttClient;
#endif
#if ARDPORTAL_ENABLE_HA
  ArdHomeAssistant _homeAssistant;
#endif
#if ARDPORTAL_ENABLE_OTA
  ArdOta _ota;
#endif

  /**
   * @brief Check whether a connection trial or its pending save is active.
   * @return True while the corresponding trial state is pending; false otherwise.
   */
  bool mqttTrialActive() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.trialActive();
#else
    return false;
#endif
  }
  /**
   * @brief Check ota active.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool otaActive() const {
#if ARDPORTAL_ENABLE_OTA
    return _ota.active();
#else
    return false;
#endif
  }
  /**
   * @brief Close the MQTT transport and clear its protocol state.
   * @return No value.
   */
  void closeMqtt() {
#if ARDPORTAL_ENABLE_MQTT
    _mqttClient.closeMqtt();
#endif
  }
  /**
   * @brief Read the revision counter for completed MQTT connection trials.
   * @return Revision counter incremented on a completed MQTT trial.
   */
  uint32_t mqttTrialRevision() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.trialRevision();
#else
    return 0;
#endif
  }
  /**
   * @brief Read the latest MQTT connection trial result code.
   * @return The latest MQTT trial result code.
   */
  uint8_t mqttTrialResult() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.trialResult();
#else
    return 0;
#endif
  }
  /**
   * @brief Expose the latest MQTT connection trial result text.
   * @return Reference to the latest MQTT trial result message.
   */
  String mqttTrialMessage() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.trialMessage();
#else
    return String();
#endif
  }
  Options _options;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  ArdDynamicPages _dynamic;
#endif
  AppConfigValueChangedCallback _appChanged;
  uint32_t _appRevision = 0;
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
  AppConfigCommandCallback _appCommand;
#endif
  /**
   * @brief Handle an HTTP request for dynamic page metadata or application values.
   * @param method HTTP request method.
   * @param path Journal file path.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool dynamicHttp(const String& method, const String& path);
  String _apName, _apPassword, _ntpServer1, _ntpServer2;
  PortalConfig _config, _pending;
  ArdFS _storage;
  ArdJsonCodec _json;
  DNSServer _dns;
  ArdJSON::JSONVar _appConfig = ArdJSON::JSONVar::object(), _pendingApp = ArdJSON::JSONVar::object();
  bool _pendingAppPatch=false;
  String _saveDocumentPrefix;
  size_t _saveDocumentAppBytes=0;
  bool scheduleAppPatch(ArdJSON::JSONVar patch,ChangeSource source,bool coalesceSource=false);
  void applyPendingApp();
  String saveDocumentSlice(size_t offset,size_t maximum) const;

  PortalConfigChangedCallback _changed;
  PortalAndAppConfigSavedCallback _saved;
  RestartCallback _beforeRestart;
  PortalAndAppConfigReadyCallback _readyCallback;
  ChangeSource _saveSource = ChangeSource::Application;
  bool _configurationReady = false, _loadStarted = false, _savePending = false, _saveQueued = false;
  bool _forceSave = false, _httpWaitingStorage = false, _resetAfterSave = false;
  bool _hasCommitted = false, _notifyPending = false, _saveHasChanges = false;
  uint32_t _dirtySince = 0, _lastCommit = 0;
  /**
   * @brief Advance pending storage operations and the restart save sequence.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceStorage(uint32_t now);
  /**
   * @brief Apply the initial storage result and schedule the ready callback.
   * @param result Operation result to inspect or return.
   * @return No value.
   */
  void finishLoad(const ArdFS::Result& result);
  /**
   * @brief Schedule persistence of the current portal and application configuration.
   * @param config Portal settings to validate and apply.
   * @param app Application configuration object.
   * @param source Input source or origin of a configuration change, as indicated by its type.
   * @param immediate Whether to skip save debounce.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool scheduleConfig(const PortalConfig& config, const ArdJSON::JSONVar& app, ChangeSource source, bool immediate);
  /**
   * @brief Report the persistent save result and advance any waiting restart.
   * @param result Operation result to inspect or return.
   * @return No value.
   */
  void finishSave(const ArdFS::Result& result);
  /**
   * @brief Notify the application of a portal configuration change and its source.
   * @return No value.
   */
  void notifyConfig();
  bool _wifiConnectAfterSave = false;
  bool _storageMounted = false;
  String _storageError;

  WiFiServer _server{80};
  WiFiClient _http;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  bool _httpDynamicPages = false, _httpDynamicPageStarted = false, _httpPortalTail = false;
  String _httpDynamicText;
  // 0: definition list, 1: selected definition, 2: selected state, 3: menu catalog.
  uint8_t _httpDynamicMode=0, _httpDynamicStage=0;
  size_t _httpDynamicField=0;
  bool _httpDynamicConditionFirst=true;
  uint32_t _httpDynamicRevision=0;
  /**
   * @brief Build the next bounded fragment of a streamed dynamic-page response.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  enum class DynamicHttpPart { Ready, RetryLater, Finished, Failed };
  DynamicHttpPart prepareDynamicHttpPart();
  bool dynamicHttpMemoryReady();
  bool reserveDynamicHttpMemory(size_t bytes,size_t block);
  bool buildDynamicHttpPart();
  size_t _httpDynamicPageIndex = 0, _httpDynamicPageCount = 0, _httpDynamicPageOffset = 0, _httpDynamicPageLength = 0;
#endif
#if ARDPORTAL_ENABLE_CONSOLE
  struct ConsoleLine { uint32_t id = 0; bool mqtt = false; String text; };
  static constexpr size_t ConsoleCapacity = 16;
  ConsoleLine _console[ConsoleCapacity]; // MQTT history.
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  ConsoleLine _consoleMessages[ConsoleCapacity];
  uint32_t _consoleMessagesCount = 0;
#endif
  uint32_t _consoleMqttCount = 0;
  size_t _consoleHistoryBudget = 4096; // Per channel, never shared.
  uint32_t _consoleId = 0;
#endif
#if ARDPORTAL_SUPPORT_WEBSOCKET
  struct WebSocketState {
    WiFiClient client;
    uint8_t* rx;size_t capacity,rxSize=0;
    String tx,pong;size_t txOffset=0;
    uint32_t since=0,frameSince=0,pingSince=0,statusSince=0,appRevision=0,cursor=0;
    bool statusSent=false,mqtt=false,closing=false,pingPending=false,appSent=false,pongPending=false;
    const bool logs;
    /**
     * @brief Initialize this instance and its owned state.
     * @param buffer Response buffer whose contents are transferred.
     * @param size Number of bytes or elements.
     * @param console Whether Console UI is included.
     * @return No value.
     */
    WebSocketState(uint8_t* buffer,size_t size,bool console):rx(buffer),capacity(size),logs(console){}
  };
  uint8_t _eventWsRx[136];
  WebSocketState _eventWs{_eventWsRx,sizeof(_eventWsRx),false};
#if ARDPORTAL_ENABLE_CONSOLE
  uint8_t _consoleWsRx[1032];
  WebSocketState _consoleWs{_consoleWsRx,sizeof(_consoleWsRx),true};
#endif
  bool _wsUpgrade=false,_wsUpgradeConsole=false;
  /**
   * @brief Validate the HTTP upgrade request and create a WebSocket session.
   * @return Reference to the requested stored value or component.
   */
  WebSocketState& upgradeWebSocket();
  /**
   * @brief Advance frame reception, transmission and WebSocket liveness checks.
   * @param ws WebSocket session to service.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceWebSocket(WebSocketState& ws,uint32_t now);
  /**
   * @brief Close the WebSocket client and clear its queued frames.
   * @param ws WebSocket session to service.
   * @return No value.
   */
  void closeWebSocket(WebSocketState& ws);
  /**
   * @brief Queue a WebSocket frame for cooperative transmission.
   * @param ws WebSocket session to service.
   * @param opcode WebSocket frame opcode.
   * @param payload Message bytes or text to send or decode.
   * @return No value.
   */
  void queueWebSocket(WebSocketState& ws,uint8_t opcode,const String& payload);
#endif
  uint32_t _minimumFreeHeap = UINT32_MAX, _minimumFreeDma = UINT32_MAX;
  /**
   * @brief Format a Console record with its timestamp and diagnostic text.
   * @param mqtt Whether MQTT UI is included.
   * @param text Text to read, encode or display.
   * @return No value.
   */
  void consoleLine(bool mqtt, const String& text);
  /**
   * @brief Append MQTT traffic to its separate Console history when the console is enabled.
   * @param direction MQTT traffic direction displayed in Console.
   * @param topic MQTT topic to publish, subscribe or match.
   * @param payload Message bytes or text to send or decode.
   * @param length Number of bytes or elements to process.
   * @return No value.
   */
  void logMqtt(const String& direction, const String& topic, const uint8_t* payload, size_t length);
  /**
   * @brief Advance frame reception, transmission and WebSocket liveness checks.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceWebSocket(uint32_t now);
  /**
   * @brief Handle a JSON command received from a portal WebSocket client.
   * @param command Command identifier or MQTT packet header.
   * @return No value.
   */
  void websocketCommand(const String& command);
  WifiState _wifiState = WifiState::NoCredentials;

#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  unsigned _diagnosticApStations = 0;
#endif
  bool _started = false, _apActive = false, _storageOK = false, _pendingReady = false;
  bool _attempting = false, _wasConnected = false, _scanRequested = false, _scanning = false;
  uint32_t _wifiRevision = 0;
  uint8_t _wifiResult = 0; // 0 pending, 1 connected, 2 failed; retained across retries.
  uint32_t _wifiSince = 0, _retrySince = 0;

  String _networks = "[]";
  uint32_t _scanId = 0, _scanCompletedId = 0;
  String _request, _response;
  const char* _page = nullptr;
  size_t _responseOffset = 0, _pageLength = 0, _pageOffset = 0;
  const ArdAssetChunk* _assetChunks = nullptr;
  uint16_t _assetCount = 0, _assetIndex = 0;
  uint8_t _assetFooter[8] = {}, _assetFooterOffset = 8;
  /**
   * @brief Prepare a static asset for an HTTP response.
   * @param chunks Chunked immutable asset descriptor.
   * @param count Number of items or bytes to process.
   * @return Prepare a static asset for an HTTP response.
   */
  size_t beginAsset(const ArdAssetChunk* chunks,size_t count);
  uint32_t _httpSince = 0;
  bool _httpConfigUpload = false;
  bool _rebootPending = false, _beforeRestartCalled = false, _beforeRestartRunning = false;
  RestartReason _restartReason = RestartReason::Portal;
  /**
   * @brief Schedule the restart sequence and record its reason.
   * @param reason Restart or failure reason.
   * @return No value.
   */
  void scheduleRestart(RestartReason reason);

  uint32_t _rebootSince = 0;
  uint64_t _uptimeMs = 0;
  uint32_t _lastLoopMs = 0;
  /**
   * @brief Build the device and firmware information JSON document.
   * @return The resulting text; an empty value indicates no available text or failure where applicable.
   */
  String infoJson();
  /**
   * @brief Update device naming and MQTT/Home Assistant identity settings.
   * @return No value.
   */
  void configureIdentity();
  /**
   * @brief Start a station connection using the supplied Wi-Fi credentials.
   * @return No value.
   */
  void connectWifi();
  /**
   * @brief Start ap.
   * @return No value.
   */
  void startAP();
  /**
   * @brief Advance station connection trials, fallback AP and Wi-Fi diagnostics.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceWifi(uint32_t now);
  /**
   * @brief Advance Wi-Fi scanning and collect completed network results.
   * @return No value.
   */
  void serviceScan();
  /**
   * @brief Advance active HTTP clients and dispatch complete requests.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceHttp(uint32_t now);
  /**
   * @brief Advance HTTP socket input and output without waiting for a complete response.
   * @param now Current time used to evaluate deadlines.
   * @return No value.
   */
  void serviceHttpIo(uint32_t now);
  bool _httpRequestReady=false;
  /**
   * @brief Route a completed HTTP request to its endpoint handler.
   * @return No value.
   */
  void handleHttp();
  /**
   * @brief Handle the built-in configuration, console and maintenance HTTP endpoints.
   * @param method HTTP request method.
   * @param path Journal file path.
   * @return No value.
   */
  void handleBuiltinHttp(const String& method,const String& path);
  /**
   * @brief Build the HTTP status and content headers for a response.
   * @param code Protocol, language or error code.
   * @param type JSON, control or protocol type being examined.
   * @param length Number of bytes or elements to process.
   * @param gzip Whether the asset uses gzip content encoding.
   * @param utf8 Whether the message should be sent as UTF-8 text.
   * @return No value.
   */
  void responseHeader(int code, const char* type, size_t length, bool gzip = false, bool utf8 = false);
  /**
   * @brief Prepare an HTTP response for the current client.
   * @param code Protocol, language or error code.
   * @param type JSON, control or protocol type being examined.
   * @param body HTTP or MQTT message body.
   * @return No value.
   */
  void reply(int code, const char* type, const String& body);
  /**
   * @brief Send a JSON status message to an HTTP client.
   * @param code Protocol, language or error code.
   * @param key Configuration key or JSON object member name.
   * @return No value.
   */
  void replyMessage(int code, ArdUILanguageData::Key key);
  /**
   * @brief Close the HTTP client and release its request/response state.
   * @return No value.
   */
  void closeHttp();
};
