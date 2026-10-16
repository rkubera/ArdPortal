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
#if ARDPORTAL_ENABLE_WWW
#include <DNSServer.h>
#endif
#include "ArdFS.h"
#include "ArdJSON.h"
#include "JsonCodec.h"
#include "AppConfigRegistrationError.h"
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "DynamicPages.h"
#include "AppConfigPageRegistration.h"
#endif
#include "PortalTypes.h"
#if ARDPORTAL_ENABLE_ZIGBEE
#include "ArdZigbee.h"
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "ArdAppControls.h"
#endif
#if ARDPORTAL_ENABLE_MQTT
#include "ArdMqtt.h"
#endif
#if ARDPORTAL_ENABLE_HA
#include "ArdHomeAssistant.h"
#endif
#if ARDPORTAL_HTTP_OTA_SUPPORT
#include "ArdOta.h"
#endif
#if defined(ESP32) && defined(CONFIG_IDF_TARGET_ESP32H2)
#include <NetworkClient.h>
#include <NetworkServer.h>
using WiFiClient=NetworkClient;
using WiFiServer=NetworkServer;
#elif defined(ESP32)
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
  #if !ARDPORTAL_ENABLE_WIFI
private:
  #endif
  enum class WifiState { NoCredentials, Connecting, Connected, FallbackAP, Disconnected };
public:
#if !ARDPORTAL_ENABLE_MQTT
private:
#endif
  using MqttState = ArdMqttState;
public:
  using ChangeSource = ArdPortalChangeSource;
  using RestartReason = ArdPortalRestartReason;
#if ARDPORTAL_ENABLE_ZIGBEE
  using ZigbeeOperation = ArdZigbee::Operation;
  using ZigbeeState = ArdZigbee::State;
  using ZigbeeOptions = ArdZigbee::Options;
  /** @brief Start joining an open network. @return True if accepted; final result arrives through the Zigbee callback. */
  bool startZigbeePairing();
  /** @brief Stop pairing and scheduled retries. @return True if accepted; callback reports cancellation. */
  bool stopZigbeePairing();
  /** @brief Rejoin using saved Zigbee identity. @return True if accepted. */
  bool rejoinZigbee();
  /** @brief Clear Zigbee pairing only and join a new network. @return True if accepted. */
  bool resetZigbeePairing();
  /** @brief Set operation timeout/retry options. @param options Limits in milliseconds and maximum attempts. @return True for valid limits; false leaves existing options unchanged. */
  bool setZigbeeOptions(const ZigbeeOptions& options){if(!ArdZigbee::validOptions(options))return false;_zigbeeOptions=options;return true;}
  /** @brief Install operation result handler. @param callback Receives operation, success and error from loop(). @return No value. */
  void onZigbeeOperationFinished(ArdZigbee::FinishedCallback callback){_zigbee.onFinished(callback);}
  /** @brief Install Zigbee state handler. @param callback Receives cached state changes from loop(). @return No value. */
  void onZigbeeStateChanged(ArdZigbee::StateCallback callback){_zigbee.onState(callback);}
  /** @brief Get Zigbee network state. @return Current state. */
  ZigbeeState zigbeeState()const{return _zigbee.state();}
#if ARDPORTAL_ZIGBEE_EXPORT_SUPPORT
  using ZigbeeExportWriter=std::function<bool(const char*,size_t)>;
  using ZigbeeExportFinished=std::function<void(bool,const String&)>;
  /** @brief Check that frozen endpoint metadata is ready for export. @return True after successful registration/native startup. */
  bool zigbeeConverterExportReady()const;
  /** @brief Check an API export job. @return True until completion/cancellation is delivered from loop. */
  bool zigbeeConverterExportBusy()const{return _apiExportStage!=0;}
  /** @brief Request cancellation; completion arrives from loop. @return True if a job exists. */
  bool cancelZigbeeConverterExport(){if(!zigbeeConverterExportBusy())return false;_apiExportCancel=true;return true;}
#if ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT
  /** @brief Queue streamed Zigbee2MQTT export without HTTP. @param writer Accept a complete borrowed fragment (max 256 bytes) or return false without consuming it. @param finished Completion/error callback. @return True if accepted; continue loop. */
  bool startZigbee2MqttConverterExport(ZigbeeExportWriter writer,ZigbeeExportFinished finished=nullptr);
#endif
#if ARDPORTAL_ZHA_QUIRK_SUPPORT
  /** @brief Queue streamed ZHA Python export without HTTP. @param writer Accept a complete borrowed fragment (max 256 bytes) or return false without consuming it. @param finished Completion/error callback. @return True if accepted; continue loop. */
  bool startZhaQuirkExport(ZigbeeExportWriter writer,ZigbeeExportFinished finished=nullptr);
#endif
#endif

#endif
#if !ARDPORTAL_ENABLE_WIFI
private:
#endif
  enum class ApPolicy { Automatic, AlwaysOn, Disabled };
public:
#if ARDPORTAL_ENABLE_WIFI
  using WifiStateCallback=std::function<void(WifiState)>;
  using ApStateCallback=std::function<void(bool)>;
  using WifiScanCallback=std::function<void(bool,const String&)>;
  /** @brief Connect using saved credentials and resume retries. @return False when not ready, busy or missing credentials. */
  bool connectWifiStation();
  /** @brief Disconnect station and suspend retries. @return False during an active configuration/OTA operation. */
  bool disconnectWifiStation();
  /** @brief Set automatic station retry policy. @param enabled Retry permission. @return No value. */
  void setWifiAutoReconnect(bool enabled){_wifiAutoReconnect=ARDPORTAL_ENABLE_WIFI&&enabled;}
  /** @brief Read station retry policy. @return True when automatic retries are enabled. */
  bool wifiAutoReconnect()const{return _wifiAutoReconnect;}
  /** @brief Install loop-owned station state notifications. @param callback State handler. @return No value. */
  void onWifiStateChanged(WifiStateCallback callback){_wifiStateCallback=std::move(callback);}
  /** @brief Queue an asynchronous Wi-Fi scan. @return False when busy or not ready. */
  bool scanWifiNetworks();
  /** @brief Check scan activity. @return True while queued or running. */
  bool wifiScanBusy()const{return _scanRequested||_scanning;}
  /** @brief Read last scan JSON. @return Borrowed array of SSID/RSSI/channel/security objects. */
  const String& wifiNetworksJson()const{return _networks;}
  /** @brief Install scan completion handler. @param callback Receives success and borrowed results. @return No value. */
  void onWifiScanFinished(WifiScanCallback callback){_wifiScanCallback=std::move(callback);}
  /** @brief Install loop-owned AP start/stop notifications. @param callback Receives active state. @return No value. */
  void onApStateChanged(ApStateCallback callback){_apStateCallback=std::move(callback);}
  /** @brief Configure AP lifetime policy. @param policy Automatic fallback, forced on or disabled. @return False while configuration/OTA work prevents changes. */
  bool setApPolicy(ApPolicy policy);
  /** @brief Read AP lifetime policy. @return Current policy. */
  ApPolicy apPolicy()const{return _apPolicy;}
  /** @brief Start AP and keep it enabled. @return True when active. */
  bool startAccessPoint(){return setApPolicy(ApPolicy::AlwaysOn);}
  /** @brief Stop AP and disable automatic fallback. @return True when stopped. */
  bool stopAccessPoint(){return setApPolicy(ApPolicy::Disabled);}
#endif
public:
#if ARDPORTAL_ENABLE_ZIGBEE
  /** @brief Expose native Zigbee status and policy API. @return Owned component reference. */
  ArdZigbee& zigbee(){return _zigbee;}
  /** @brief Read native Zigbee status. @return Owned immutable component reference. */
  const ArdZigbee& zigbee()const{return _zigbee;}
#endif
  using RestartCallback = std::function<void(RestartReason)>;
  using BeforeFactoryResetCallback = std::function<void()>;
  using PortalConfigChangedCallback = std::function<void(const PortalConfig&, ChangeSource)>;
  using PortalAndAppConfigSavedCallback = std::function<void(bool, const String&)>;
  using PortalAndAppConfigReadyCallback = std::function<void(bool)>;
#if ARDPORTAL_ENABLE_MQTT
  using MessageCallback = std::function<void(const String&, const uint8_t*, size_t)>;
#endif
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
   * @brief Install the callback run before a confirmed factory reset, even if settings are already default.
   * @param callback Synchronous handler called once before default settings are scheduled for storage. An empty handler disables it.
   * @return No value. Ordinary restarts and storage formatting do not invoke this callback.
   */
  void onBeforeFactoryReset(BeforeFactoryResetCallback callback) { _beforeFactoryReset = callback; }
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
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
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
#if ARDPORTAL_ENABLE_HA
  bool addAppConfigEntity(const String& definition);
  /**
   * @brief Register a Home Assistant entity without adding a page to the portal navigation.
   * @param definition JSON definition to register; flash sources must remain valid for the portal lifetime.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool addAppConfigEntity(const __FlashStringHelper* definition);
#endif
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
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
  #endif
  bool setAppConfigStateValue(const char* key, const ArdJSON::JSONVar& value, bool publishMqtt = true);
  /**
   * @brief Mark an application field for MQTT state publication.
   * @param key Configuration key or JSON object member name.
   * @return True when the field is found and queued; false for an unknown or unsupported field.
   */
#if ARDPORTAL_ENABLE_MQTT
  bool queueAppConfigStatePublish(const char* key);
#endif
  /**
   * @brief Emit a transient application event through the field MQTT topic.
   * @param key Configuration key or JSON object member name.
   * @param value Input value, or output destination when passed by mutable reference.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
#if ARDPORTAL_CONTROL_SUPPORT_EVENTS
  bool emitAppConfigEvent(const char* key, const ArdJSON::JSONVar& value);
#endif
#endif
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
#if !ARDPORTAL_ENABLE_CONSOLE_MESSAGES
private:
#endif
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
public:
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
#if ARDPORTAL_ENABLE_MQTT
  void onMqttMessage(MessageCallback callback) {
#if ARDPORTAL_ENABLE_MQTT
    _mqttClient.onMessage(std::move(callback));
#else
    (void)callback;
#endif
  }
#endif
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
#if ARDPORTAL_HTTP_OTA_SUPPORT
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
  /** @brief Set application firmware version before begin(); default is "1.0".
   * @param version Nonempty version string, at most 16 UTF-8 bytes (Zigbee SWBuildID limit).
   * @return False for invalid input or when initialization has already started. */
  bool setFirmwareVersion(const char* version);
  /** @brief Read application firmware version. @return Borrowed string owned by the portal. */
  const char* getFirmwareVersion() const { return _firmwareVersion+1; }

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
#if !ARDPORTAL_ENABLE_WIFI
private:
#endif
  bool wifiConnected() const {
#if ARDPORTAL_ENABLE_WIFI
return WiFi.status() == WL_CONNECTED;
#else
return false;
#endif
}
public:

  /**
   * @brief Check whether the portal MQTT session is connected.
   * @return True when the MQTT protocol session is connected.
   */
#if !ARDPORTAL_ENABLE_MQTT
private:
#endif
  bool mqttConnected() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.connected();
#else
    return false;
#endif
  }
public:

  /**
   * @brief Read the portal Wi-Fi connection state.
   * @return The current portal Wi-Fi state.
   */
#if !ARDPORTAL_ENABLE_WIFI
private:
#endif
  WifiState wifiState() const { return _wifiState; }
public:

  /**
   * @brief Read the MQTT connection state.
   * @return The current MQTT state.
   */
#if !ARDPORTAL_ENABLE_MQTT
private:
#endif
  MqttState mqttState() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.state();
#else
    return MqttState::Disabled;
#endif
  }
public:

  /**
   * @brief Check whether the configuration access point is active.
   * @return True when the configuration AP is active.
   */
#if !ARDPORTAL_ENABLE_WIFI
private:
#endif
  bool apActive() const { return _apActive; }
public:

  /**
   * @brief Check whether the clock is valid enough for TLS certificate verification.
   * @return True when system time is valid for certificate checks.
   */
#if !ARDPORTAL_ENABLE_MQTT_TLS
private:
#endif
  bool tlsClockReady() const { return time(nullptr) >= 1704067200; }
public:
 // 2024-01-01
  /**
   * @brief Check portal and app config storage ok.
   * @return True on success; false if validation, resource allocation or the operation fails.
   */
  bool portalAndAppConfigStorageOK() const { return _storageOK; }
  /**
   * @brief Read the Wi-Fi station IP address.
   * @return The Wi-Fi station IP address.
   */
#if !ARDPORTAL_ENABLE_WIFI
private:
#endif
  IPAddress localIP() const {
#if ARDPORTAL_ENABLE_WIFI
return WiFi.localIP();
#else
return IPAddress();
#endif
}
public:

  /**
   * @brief Read the configuration access point IP address.
   * @return The AP IP address.
   */
#if !ARDPORTAL_ENABLE_WIFI
private:
#endif
  IPAddress apIP() const {
#if ARDPORTAL_ENABLE_WIFI
return WiFi.softAPIP();
#else
return IPAddress();
#endif
}
public:

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
#if ARDPORTAL_ENABLE_MQTT
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
#endif
  static bool validPortalConfig(const PortalConfig& config);
private:
  bool _wifiAutoReconnect=ARDPORTAL_ENABLE_WIFI;
  ApPolicy _apPolicy=ApPolicy::Automatic;
#if ARDPORTAL_ENABLE_WIFI
  WifiStateCallback _wifiStateCallback;
  WifiScanCallback _wifiScanCallback;
  ApStateCallback _apStateCallback;
  bool _apiApActive=false;
  WifiState _apiWifiState=WifiState::NoCredentials;
  uint32_t _apiScanCompletedId=0;
  bool _scanSucceeded=false;
#endif
  uint32_t _portalConfigRevision=0;
  /** @brief Deliver deferred network callbacks from loop. @return No value. */
  void serviceNetworkApi();
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  ArdAppConfigRegistrationError _appConfigRegistrationError;
  AppConfigPageRegistrationFinishedCallback _appConfigPageRegistrationFinished;
  AppConfigPageRegistrationState _appConfigPageRegistrationState=AppConfigPageRegistrationState::Idle;
  size_t _appConfigPageRegistrationProcessed=0,_appConfigPageRegistrationTotal=0;
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
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  bool appConfigRegistrationFailed(const char* stage,const String& field,const String& reason) {
    _appConfigRegistrationError={stage,field,reason};
    log(String("AppConfig registration: stage=")+stage+" field="+field+" reason="+reason);return false;
  }
#endif
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
#if ARDPORTAL_HTTP_OTA_SUPPORT
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
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
    if(_zigbee.otaActive())return true;
#endif
#if ARDPORTAL_HTTP_OTA_SUPPORT
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
  char _firmwareVersion[18]="\x03" "1.0"; // ZCL length prefix plus bounded text and terminator.
  Options _options;
#if ARDPORTAL_ENABLE_ZIGBEE
  ArdZigbee _zigbee;
  ZigbeeOptions _zigbeeOptions;
  bool _zigbeeBeginTried=false;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  size_t _zigbeeBuildIndex=0,_zigbeeSeenFields=0;
  uint8_t _zigbeeReportEndpoint=0;
  uint16_t _zigbeeBuildPart=0;
  uint32_t _zigbeeRegistrationSince=0,_zigbeeRetryAt=0,_zigbeeReportReadyAt=0;
  bool _zigbeeWasConnected=false,_zigbeeMappingFailed=false,_zigbeeRetryPending=false;
  ArdZigbeeFields::Command _zigbeePendingCommand;
  bool _zigbeeHasCommand=false,_zigbeeDrainingRestart=false;
#if ARDPORTAL_ZIGBEE_OTA_SUPPORT
  bool _zigbeeOtaProgress=false;
  /** @brief Mirror native OTA progress into its AppConfig field without writing flash. @param active Reception is active. @return True when mirrored or no update field exists. */
  bool setZigbeeOtaProgress(bool active);
#endif
  ArdJSON::JSONVar _zigbeeAppQueue=ArdJSON::JSONVar::object();
  /** @brief Stage native endpoints, apply queued commands and report one field per loop. @param now Current milliseconds. @return No value. */
  void serviceZigbeeControls(uint32_t now);
  /** @brief Mark only the mapped endpoint of a changed AppConfig field. @param key Changed field ID. @return No value. */
  void markZigbeeStateDirty(const String& key);
  /** @brief Apply a Zigbee setting and queue its optional persistent write. @param key Field ID. @param value Valid setting. @return True if accepted. */
  bool acceptZigbeeState(const String& key,const ArdJSON::JSONVar& value);
#endif
#if ARDPORTAL_ENABLE_WWW
  /** @brief Handle Zigbee HTTP requests. @param method HTTP method. @param path Resource path. @return True if handled. */
  bool handleZigbeeHttp(const String& method,const String& path);
  /** @brief Build Zigbee status. @return JSON status document. */
  String zigbeeStatusJson();
#endif
#if ARDPORTAL_ZIGBEE_LAYOUT_SUPPORT
#if ARDPORTAL_ZIGBEE_HTTP_EXPORT_SUPPORT
  const ArdZigbee::Control* _converterControl=nullptr;
  bool _converterDownload=false,_converterPython=false;
  uint8_t _converterStage=0;
#endif
  const ArdZigbee::Control* _converterIdentityControl=nullptr;
  uint32_t _converterHashA=2166136261UL,_converterHashB=0x9e3779b9UL;
  bool _converterIdentityStarted=false,_converterLayoutReady=false;
  /** @brief Describe one endpoint for export and layout identity. @param control Registered endpoint. @return JSON or empty on allocation failure. */
  String zigbeeConverterControlJson(const ArdZigbee::Control& control);
  /** @brief Produce one converter fragment. @return False to retry after memory pressure. */
#if ARDPORTAL_ZIGBEE_HTTP_EXPORT_SUPPORT
  bool prepareZigbeeConverterPart();
#endif
#if ARDPORTAL_ZIGBEE_EXPORT_SUPPORT
  ZigbeeExportWriter _apiExportWriter;
  ZigbeeExportFinished _apiExportFinished;
  const ArdZigbee::Control* _apiExportControl=nullptr;
  String _apiExportText;
  size_t _apiExportOffset=0;
  uint32_t _apiExportProgressAt=0;
  uint8_t _apiExportStage=0;
  bool _apiExportPython=false,_apiExportFirst=true,_apiExportCancel=false;
  /** @brief Initialize one API stream. @param python Format selector. @param writer Fragment consumer. @param finished Completion handler. @return True when accepted. */
  bool beginZigbeeConverterExport(bool python,ZigbeeExportWriter writer,ZigbeeExportFinished finished);
  /** @brief Advance at most one output fragment in loop. @return No value. */
  void serviceZigbeeConverterExport();
  /** @brief Release stream state and deliver completion once. @param success Result. @param error Explanation. @return No value. */
  void finishZigbeeConverterExport(bool success,const String& error);
#endif
#endif
#endif
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
#if ARDPORTAL_ENABLE_WIFI
  String _ntpServer1, _ntpServer2;
#endif
  PortalConfig _config, _pending;
  ArdFS _storage;
  ArdJsonCodec _json;
#if ARDPORTAL_ENABLE_WWW
  DNSServer _dns;
#endif
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
  BeforeFactoryResetCallback _beforeFactoryReset;
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

#if ARDPORTAL_ENABLE_WWW
  WiFiServer _server{80};
  WiFiClient _http;
#else
  static constexpr bool _http = false;
#endif
#if ARDPORTAL_ENABLE_WWW && ARDPORTAL_ENABLE_DYNAMIC_PAGES
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
#if ARDPORTAL_ENABLE_MQTT
  ConsoleLine _console[ConsoleCapacity]; // MQTT history.
  uint32_t _consoleMqttCount = 0;
#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  ConsoleLine _consoleMessages[ConsoleCapacity];
  uint32_t _consoleMessagesCount = 0;
#endif
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
#if ARDPORTAL_ENABLE_ZIGBEE
  uint8_t _zigbeeWsRx[136];
  WebSocketState _zigbeeWs{_zigbeeWsRx,sizeof(_zigbeeWsRx),false};
  bool _wsUpgradeZigbee=false;
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
  #if ARDPORTAL_ENABLE_WWW
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
#else
  static constexpr bool _httpConfigUpload = false;
#endif
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
#if ARDPORTAL_ENABLE_WWW
  bool _httpRequestReady=false;
#endif
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
