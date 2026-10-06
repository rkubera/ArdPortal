// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
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
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "DynamicPages.h"
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
  ArdPortal(ArdPortalBuildTag<ARDPORTAL_FEATURE_MASK> = {});
  ArdPortal(const ArdPortal&) = delete;
  ArdPortal& operator=(const ArdPortal&) = delete;
  ArdPortal(ArdPortal&&) = delete;
  ArdPortal& operator=(ArdPortal&&) = delete;
  using PortalConfig = ArdPortalConfig;
  struct Options {
    const char* deviceName = nullptr; // Default: ArdUI- + chip ID; also the AP SSID.
    const char* apPassword = "1234567890";
    const char* ntpServer1 = "pool.ntp.org";
    const char* ntpServer2 = "time.cloudflare.com";
    uint32_t appStateIntervalMs = 600000; // Retained application state refresh.
    uint32_t discoveryIntervalMs = 300000; // Retained Home Assistant discovery refresh.
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
  void onPortalConfigChanged(PortalConfigChangedCallback callback) { _changed = callback; }
  void onBeforeRestart(RestartCallback callback) { _beforeRestart = callback; }
  void onPortalAndAppConfigSaved(PortalAndAppConfigSavedCallback callback) { _saved = callback; }
  void onPortalAndAppConfigReady(PortalAndAppConfigReadyCallback callback) { _readyCallback = callback; }
  bool portalAndAppConfigReady() const { return _configurationReady; }
  bool portalAndAppConfigBusy() const { return _storage.busy() || _savePending || _pendingReady; }
  // true = accepted in RAM; onPortalAndAppConfigSaved signals durable completion.
  bool setPortalConfig(const PortalConfig& config);
  bool setPortalConfigDeviceManufacturer(const String& manufacturer);
  bool setPortalConfigDeviceDescription(const String& description);
  ArdJSON::JSONVar getAppConfigValue(const char* key) const;
  bool setAppConfigValue(const char* key, const ArdJSON::JSONVar& value);
  bool removeAppConfigValue(const char* key);
  bool addAppConfigPage(const String& definition);
  bool addAppConfigPage(const __FlashStringHelper* definition); // Source must remain valid for the portal lifetime.
  using AppConfigCommandCallback = std::function<bool(const String&, const String&, const ArdJSON::JSONVar&, ChangeSource)>;
  void onAppConfigCommand(AppConfigCommandCallback callback) {
#if ARDPORTAL_CONTROL_SUPPORT_ACTIONS
    _appCommand=callback;
#else
    (void)callback;
#endif
  }
  bool setAppConfigStateValue(const char* key, const ArdJSON::JSONVar& value, bool publishMqtt = true);
  bool queueAppConfigStatePublish(const char* key);
  bool emitAppConfigEvent(const char* key, const ArdJSON::JSONVar& value);
  using AppConfigValueChangedCallback = std::function<void(const String&, const ArdJSON::JSONVar&, ChangeSource)>;
  void onAppConfigValueChanged(AppConfigValueChangedCallback callback) { _appChanged = callback; }
  bool flushPortalAndAppConfig(); // Skip debounce, never perform flash I/O here.
  void log(const String& message); // Diagnostic messages in Console > Messages.
  void logMessage(const String& message) { log(message); } // Explicit diagnostic logging; no Serial interception.
  const String& portalAndAppConfigError() const { return _storageError; }
  static String chipId();
  static String defaultDeviceName() { return "ArdUI-" + chipId(); }
  void onMqttMessage(MessageCallback callback) {
#if ARDPORTAL_ENABLE_MQTT
    _mqttClient.onMessage(std::move(callback));
#else
    (void)callback;
#endif
  }
#if ARDPORTAL_ENABLE_MQTT
  ArdMqtt& mqtt() { return _mqttClient; }
  const ArdMqtt& mqtt() const { return _mqttClient; }
#endif
#if ARDPORTAL_ENABLE_HA
  ArdHomeAssistant& homeAssistant() { return _homeAssistant; }
  const ArdHomeAssistant& homeAssistant() const { return _homeAssistant; }
#endif
#if ARDPORTAL_ENABLE_OTA
  ArdOta& ota() { return _ota; }
  const ArdOta& ota() const { return _ota; }
#endif
  bool begin();
  bool begin(const Options& options);
  void loop();
  bool wifiConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool mqttConnected() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.connected();
#else
    return false;
#endif
  }
  WifiState wifiState() const { return _wifiState; }
  MqttState mqttState() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.state();
#else
    return MqttState::Disabled;
#endif
  }
  bool apActive() const { return _apActive; }
  bool tlsClockReady() const { return time(nullptr) >= 1704067200; } // 2024-01-01
  bool portalAndAppConfigStorageOK() const { return _storageOK; }
  IPAddress localIP() const { return WiFi.localIP(); }
  IPAddress apIP() const { return WiFi.softAPIP(); }
  const PortalConfig& getPortalConfig() const { return _config; }
  // QoS 0; false means disconnected, invalid arguments or a busy send buffer.
  String mqttTopic(const char* kind, const char* command) const;
  bool validMqttTopic(const String& topic, bool subscription = false) const;
  bool publish(const char* topic, const char* payload, bool retain = false);
  bool subscribe(const char* topic);
  static bool validPortalConfig(const PortalConfig& config);
private:
  friend class ArdMqtt;
  friend class ArdHomeAssistant;
  friend class ArdOta;
  friend class ArdAppControls;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  ArdAppControls _appControls;
#endif
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

  bool mqttTrialActive() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.trialActive();
#else
    return false;
#endif
  }
  bool otaActive() const {
#if ARDPORTAL_ENABLE_OTA
    return _ota.active();
#else
    return false;
#endif
  }
  void closeMqtt() {
#if ARDPORTAL_ENABLE_MQTT
    _mqttClient.closeMqtt();
#endif
  }
  uint32_t mqttTrialRevision() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.trialRevision();
#else
    return 0;
#endif
  }
  uint8_t mqttTrialResult() const {
#if ARDPORTAL_ENABLE_MQTT
    return _mqttClient.trialResult();
#else
    return 0;
#endif
  }
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
  bool dynamicHttp(const String& method, const String& path);
  String _apName, _apPassword, _ntpServer1, _ntpServer2;
  PortalConfig _config, _pending;
  ArdFS _storage;
  ArdJsonCodec _json;
  DNSServer _dns;
  ArdJSON::JSONVar _appConfig = ArdJSON::JSONVar::object(), _pendingApp = ArdJSON::JSONVar::object();
  PortalConfigChangedCallback _changed;
  PortalAndAppConfigSavedCallback _saved;
  RestartCallback _beforeRestart;
  PortalAndAppConfigReadyCallback _readyCallback;
  ChangeSource _saveSource = ChangeSource::Application;
  bool _configurationReady = false, _loadStarted = false, _savePending = false, _saveQueued = false;
  bool _forceSave = false, _httpWaitingStorage = false, _resetAfterSave = false;
  bool _hasCommitted = false, _notifyPending = false, _saveHasChanges = false;
  uint32_t _dirtySince = 0, _lastCommit = 0;
  void serviceStorage(uint32_t now);
  void finishLoad(const ArdFS::Result& result);
  bool scheduleConfig(const PortalConfig& config, const ArdJSON::JSONVar& app, ChangeSource source, bool immediate);
  void finishSave(const ArdFS::Result& result);
  void notifyConfig();
  bool _wifiConnectAfterSave = false;
  bool _storageMounted = false;
  String _storageError;

  WiFiServer _server{80};
  WiFiClient _http;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  bool _httpDynamicPages = false, _httpDynamicPageStarted = false, _httpPortalTail = false;
  String _httpDynamicText;
  size_t _httpDynamicPageIndex = 0, _httpDynamicPageCount = 0, _httpDynamicPageOffset = 0, _httpDynamicPageLength = 0;
#endif
#if ARDPORTAL_ENABLE_CONSOLE
  struct ConsoleLine { uint32_t id = 0; bool mqtt = false; String text; };
  static constexpr size_t ConsoleCapacity = 16;
  ConsoleLine _console[ConsoleCapacity];
  size_t _consoleHistoryBudget = 8192;
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
    WebSocketState(uint8_t* buffer,size_t size,bool console):rx(buffer),capacity(size),logs(console){}
  };
  uint8_t _eventWsRx[136];
  WebSocketState _eventWs{_eventWsRx,sizeof(_eventWsRx),false};
#if ARDPORTAL_ENABLE_CONSOLE
  uint8_t _consoleWsRx[1032];
  WebSocketState _consoleWs{_consoleWsRx,sizeof(_consoleWsRx),true};
#endif
  bool _wsUpgrade=false,_wsUpgradeConsole=false;
  WebSocketState& upgradeWebSocket();
  void serviceWebSocket(WebSocketState& ws,uint32_t now);
  void closeWebSocket(WebSocketState& ws);
  void queueWebSocket(WebSocketState& ws,uint8_t opcode,const String& payload);
#endif
  uint32_t _minimumFreeHeap = UINT32_MAX;
  void consoleLine(bool mqtt, const String& text);
  void logMqtt(const String& direction, const String& topic, const uint8_t* payload, size_t length);
  void serviceWebSocket(uint32_t now);
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
  size_t beginAsset(const ArdAssetChunk* chunks,size_t count);
  uint32_t _httpSince = 0;
  bool _httpConfigUpload = false;
  bool _rebootPending = false, _beforeRestartCalled = false, _beforeRestartRunning = false;
  RestartReason _restartReason = RestartReason::Portal;
  void scheduleRestart(RestartReason reason);

  uint32_t _rebootSince = 0;
  uint64_t _uptimeMs = 0;
  uint32_t _lastLoopMs = 0;
  String infoJson();
  void configureIdentity();
  void connectWifi();
  void startAP();
  void serviceWifi(uint32_t now);
  void serviceScan();
  void serviceHttp(uint32_t now);
  void serviceHttpIo(uint32_t now);
  bool _httpRequestReady=false;
  void handleHttp();
  void handleBuiltinHttp(const String& method,const String& path);
  void responseHeader(int code, const char* type, size_t length, bool gzip = false, bool utf8 = false);
  void reply(int code, const char* type, const String& body);
  void replyMessage(int code, ArdUILanguageData::Key key);
  void closeHttp();
};
