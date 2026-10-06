// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalDeclarations.h"
#include "Language.h"
#include "PortalPage.h"
#include "PortalAssets.h"
#include "ConfigJson.h"
#include "DeviceName.h"
#if ARDPORTAL_SUPPORT_WEBSOCKET
#include "WebSocketAccept.h"
#endif
#include "NetworkIo.h"
using ArdNetworkIo::writeChunk;
using ArdNetworkIo::stopClient;
#if defined(ESP32)
#include <esp_system.h>
#include <lwip/sockets.h>
#include <errno.h>
#endif

namespace {
// The portal needs one fixed UTC format; avoid the general strftime formatter.
bool utcTimestamp(time_t value, char* output, size_t capacity) {
  if (!capacity) return false;
  output[0] = 0;
  struct tm date;
  if (!gmtime_r(&value, &date) || date.tm_year < -1900 || date.tm_year > 8099) return false;
  int length = snprintf(output, capacity, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                        date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
                        date.tm_hour, date.tm_min, date.tm_sec);
  if (length < 0 || size_t(length) >= capacity) { output[0] = 0; return false; }
  return true;
}
ArdUILanguage::Key resetReasonKey() {
#if defined(ESP32)
  switch(esp_reset_reason()) {
    case ESP_RST_POWERON: return ArdUILanguage::Key::s_180;
    case ESP_RST_EXT: return ArdUILanguage::Key::s_181;
    case ESP_RST_SW: return ArdUILanguage::Key::s_182;
    case ESP_RST_PANIC: return ArdUILanguage::Key::s_184;
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return ArdUILanguage::Key::s_183;
    case ESP_RST_DEEPSLEEP: return ArdUILanguage::Key::s_185;
    case ESP_RST_BROWNOUT: return ArdUILanguage::Key::s_186;
    default: return ArdUILanguage::Key::s_187;
  }
#else
  auto* info=ESP.getResetInfoPtr();if(!info) return ArdUILanguage::Key::s_187;
  switch(info->reason) {
    case 0: return ArdUILanguage::Key::s_180;case 1: case 3: return ArdUILanguage::Key::s_183;
    case 2: return ArdUILanguage::Key::s_184;case 4: return ArdUILanguage::Key::s_182;
    case 5: return ArdUILanguage::Key::s_185;case 6: return ArdUILanguage::Key::s_181;
    default: return ArdUILanguage::Key::s_187;
  }
#endif
}
void appendQuoted(String& out, const String& s) {
  out += '"';
  ArdCooperativeBudget budget;
  for (size_t i = 0; i < s.length(); ++i) {
    uint8_t c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 32) { char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped; }
    else out += char(c);
    budget.checkpoint();
  }
  out += '"';
}
#if ARDPORTAL_ENABLE_CONSOLE
String quote(const String& s) {
  String out; appendQuoted(out,s); return out;
}
#endif
bool number(const String& s, uint32_t& value) {
  if (!s.length() || s.length() > 10) return false;
  value = 0;
  for (size_t i = 0; i < s.length(); ++i) {
    if (s[i] < '0' || s[i] > '9') return false;
    uint32_t digit = s[i] - '0';
    if (value > (UINT32_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  return true;
}
int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool decode(const String& s, String& out) {
  out = "";
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '+') c = ' ';
    else if (c == '%') {
      if (i + 2 >= s.length() || hex(s[i+1]) < 0 || hex(s[i+2]) < 0) return false;
      c = char(hex(s[i+1]) * 16 + hex(s[i+2])); i += 2;
    }
    if (!c) return false;
    out += c;
  }
  return true;
}

}

ArdPortal::ArdPortal(ArdPortalBuildTag<ARDPORTAL_FEATURE_MASK>)
  :
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
    _appControls(*this)
#elif ARDPORTAL_ENABLE_MQTT
    _mqttClient(*this)
#elif ARDPORTAL_ENABLE_OTA
    _ota(*this)
#else
    _config()
#endif
#if ARDPORTAL_ENABLE_MQTT && ARDPORTAL_ENABLE_DYNAMIC_PAGES
  , _mqttClient(*this)
#endif
#if ARDPORTAL_ENABLE_HA
  , _homeAssistant(*this)
#endif
#if ARDPORTAL_ENABLE_OTA && (ARDPORTAL_ENABLE_DYNAMIC_PAGES || ARDPORTAL_ENABLE_MQTT)
  , _ota(*this)
#endif
{}

bool ArdPortal::validConfig(const Config& c) {
  for(size_t i=0;i<sizeof(ArdPortalJson::TextFields)/sizeof(ArdPortalJson::TextFields[0]);++i) {
    auto entry=ArdPortalJson::field(i);const String& value=c.*entry.member;
    if(value.length()>entry.limit)return false;
    for(size_t j=0;j<value.length();++j)if(value[j]==0)return false;
  }
  if(!ArdDeviceName::valid(c.deviceName))return false;
  return c.apName.length() > 0 &&
         (!c.apPassword.length() || c.apPassword.length() >= 8) && c.port != 0 && (!c.mqttTls || !c.host.length() ||
           (c.caCert.indexOf("-----BEGIN CERTIFICATE-----") >= 0 &&
            c.caCert.indexOf("-----END CERTIFICATE-----") >= 0)) &&
         (!c.mqttPassword.length() || c.user.length());
}

bool ArdPortal::begin() { return begin(Options{}); }
bool ArdPortal::begin(const Options& options) {
  if (_started) return true;
  _options = options; _apName = options.deviceName ? options.deviceName : defaultDeviceName();
  _apPassword = options.apPassword ? options.apPassword : "";
  _ntpServer1 = options.ntpServer1 ? options.ntpServer1 : "";
  _ntpServer2 = options.ntpServer2 ? options.ntpServer2 : "";
  if (!_ntpServer1.length() || _ntpServer1.length() > 253 || _ntpServer2.length() > 253 ||
      !_apName.length() || _apName.length() > 32 ||
      (_apPassword.length() && (_apPassword.length() < 8 || _apPassword.length() > 63)) ||
      !_options.wifiTimeoutMs || !_options.retryMs || !_options.mqttTimeoutMs ||
      !_options.tcpTimeoutMs || !_options.keepAliveSeconds ||
      !_options.tlsHandshakeTimeoutSeconds) return false;
  _config.deviceName = _apName;
  _apName = ArdDeviceName::ap(_config.deviceName);
  _config.apName = _apName; _config.apPassword = _apPassword;
  if (!validConfig(_config)) return false;
  WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_STA);
  // Stop any SDK/core association inherited at startup. Storage loading gives
  // the network stack time to settle before the explicit saved-config attempt.
  // Keep the radio enabled and preserve SDK credentials without flash writes.
  WiFi.disconnect(false, false);
  configureIdentity();
  _lastLoopMs = millis(); _uptimeMs = _lastLoopMs;
  _server.begin(); _started = true;_minimumFreeHeap=ESP.getFreeHeap();
  log(ArdUILanguage::text(ArdUILanguage::Key::s_188)+ArdUILanguage::text(resetReasonKey()));
  _storage.begin();
  return true; // Storage state is separate; a missing config is normal on first boot.
}
void ArdPortal::connectWifi() {
  _wifiSince=millis();
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  logMessage(String("Wi-Fi connecting: ")+_config.ssid);
#endif
  log(ArdUILanguage::text(ArdUILanguage::Key::s_209)+String(_wifiSince));
  WiFi.begin(_config.ssid.c_str(), _config.password.c_str());
  _attempting = true; _wifiState = WifiState::Connecting;
}
void ArdPortal::startAP() {
  if (!_apActive) {
    WiFi.mode(WIFI_AP_STA);
    log(ArdUILanguage::text(ArdUILanguage::Key::s_101) + _apName);
    _apActive = WiFi.softAP(_apName.c_str(), _apPassword.length() ? _apPassword.c_str() : nullptr);
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
    _diagnosticApStations = 0;
    logMessage(_apActive ? String("AP started: ")+_apName+"; IP="+WiFi.softAPIP().toString() : String("AP start failed: ")+_apName);
#endif
    if (_apActive) { _dns.setErrorReplyCode(DNSReplyCode::NoError); _dns.start(53, "*", WiFi.softAPIP()); }
  }
  _wifiState = _config.ssid.length() ? WifiState::FallbackAP : WifiState::NoCredentials;
  _retrySince = millis();
}
void ArdPortal::serviceWifi(uint32_t now) {
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
  const unsigned stations = _apActive ? WiFi.softAPgetStationNum() : 0;
  if(stations != _diagnosticApStations) {
    const bool connected = stations > _diagnosticApStations;
    logMessage(String(connected ? "AP clients connected: " : "AP clients disconnected: ")+
      String(connected ? stations-_diagnosticApStations : _diagnosticApStations-stations)+
      "; active="+String(stations));
    _diagnosticApStations = stations;
  }
#endif
  if (wifiConnected()) {
    const bool wasAttempting=_attempting;_wifiState = WifiState::Connected; _attempting = false;
    if (!_wifiResult) _wifiResult = 1;
    if (!_wasConnected) {
      log(ArdUILanguage::text(ArdUILanguage::Key::s_102) + localIP().toString());
      if(wasAttempting) log(ArdUILanguage::text(ArdUILanguage::Key::s_210)+String(uint32_t(now-_wifiSince)));
      _wasConnected = true;
#if ARDPORTAL_ENABLE_MQTT
      _mqttClient.wifiAvailable(now);
#endif
      // Start/restart background SNTP after each Wi-Fi connection, in UTC.
      // The core retries and periodically refreshes time; never wait here.
      configTime(0, 0, _ntpServer1.c_str(), _ntpServer2.length() ? _ntpServer2.c_str() : nullptr);
    }
    if (_apActive && !_http && WiFi.softAPgetStationNum() == 0) {
      if (WiFi.softAPdisconnect(true)) { log(ArdUILanguage::text(ArdUILanguage::Key::s_103)); _apActive = false; _dns.stop(); WiFi.mode(WIFI_STA); }
    }
    return;
  }
  // STA association can retune the shared radio and disconnect AP stations.
  // Keep automatic retries paused while someone is using the fallback portal.
  const bool apInUse = _apActive && WiFi.softAPgetStationNum() != 0;
  if (_wasConnected) {
    log(ArdUILanguage::text(ArdUILanguage::Key::s_104)); _wasConnected = false; closeMqtt();
    if (apInUse) startAP(); else connectWifi();
    return;
  }
  if (_attempting && uint32_t(now - _wifiSince) >= _options.wifiTimeoutMs) {
    if (!_wifiResult) _wifiResult = 2;
    log(ArdUILanguage::text(ArdUILanguage::Key::s_105));
    log(ArdUILanguage::text(ArdUILanguage::Key::s_210)+String(uint32_t(now-_wifiSince)));
    WiFi.disconnect(false, false); _attempting = false; startAP();
  } else if (!_attempting && !apInUse && !_scanning && _config.ssid.length() && uint32_t(now - _retrySince) >= _options.retryMs) connectWifi();
  else if (!_attempting && !_apActive && uint32_t(now - _retrySince) >= _options.retryMs) startAP();
}
void ArdPortal::serviceScan() {
  // Defer scans during association so the scan cannot interrupt a Wi-Fi attempt.
  if (_scanRequested && !_scanning && !_attempting) {
    _scanRequested = false; WiFi.scanDelete();
    int result = WiFi.scanNetworks(true);
    _scanning = result == WIFI_SCAN_RUNNING || result >= 0;
    if (!_scanning) { _networks = "[]"; _scanCompletedId = _scanId; log(ArdUILanguage::text(ArdUILanguage::Key::s_106)); }
  }
  if (!_scanning) return;
  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) return;
  _networks = "[";
  for (int i = 0; i < count && i < 32; ++i) {
    if (i) _networks += ',';
#if defined(ESP8266)
    bool secured = WiFi.encryptionType(i) != ENC_TYPE_NONE;
#else
    bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
#endif
    _networks += F("{\"ssid\":"); appendQuoted(_networks,WiFi.SSID(i));
    _networks += F(",\"rssi\":"); _networks += String(WiFi.RSSI(i));
    _networks += F(",\"secured\":"); _networks += secured ? "true" : "false"; _networks += '}';
  }
  _networks += ']'; WiFi.scanDelete(); _scanning = false; _scanCompletedId = _scanId;
}
void ArdPortal::scheduleRestart(RestartReason reason) {
  _restartReason = reason; _beforeRestartCalled = false;
  _rebootPending = true; _rebootSince = millis();
}
void ArdPortal::loop() {
  if (!_started || _beforeRestartRunning) return;
  uint32_t tick = millis();uint32_t freeHeap=ESP.getFreeHeap();if(freeHeap<_minimumFreeHeap)_minimumFreeHeap=freeHeap;serviceStorage(tick);
  if (_apActive) _dns.processNextRequest();
  _uptimeMs += uint32_t(tick - _lastLoopMs); _lastLoopMs = tick;
  // Apply settings only after the HTTP response has been sent/closed.
  if (_pendingReady && !_http && !_scanning) {
    bool apChanged = _config.apName != _pending.apName || _config.apPassword != _pending.apPassword;
    closeMqtt(); _config = std::move(_pending); applyAppConfig(std::move(_pendingApp)); _pendingReady = false;_pendingApp=ArdJSON::JSONVar::object();_pending.caCert=String();
    ++_wifiRevision; _wifiResult = _config.ssid.length() ? 0 : 2;
    _apName = _config.apName; _apPassword = _config.apPassword;
    configureIdentity();
    if (apChanged && _apActive) {
      // Reconfigure the active AP only after the HTTP confirmation was sent.
      _apActive = false; startAP();
    }
  
#if ARDPORTAL_ENABLE_MQTT
  _mqttClient.configurationLoaded();
#endif
    WiFi.disconnect(); _wasConnected = false; _attempting = false;
    if (_config.ssid.length()) connectWifi(); else startAP();
    _pending = Config();
    if (_notifyPending) { _notifyPending = false; notifyConfig(); }
  }

  if (_rebootPending && !_http && uint32_t(tick - _rebootSince) >= 300) {
    // Drain old transactions, including debounced saves, before running cleanup.
    if (storageBusy()) { flushConfig(); return; }
    if (!_beforeRestartCalled) {
      _beforeRestartCalled = true;
      if (_restartReason != RestartReason::FactoryReset && _beforeRestart) {
        _beforeRestartRunning = true;
        _beforeRestart(_restartReason);
        _beforeRestartRunning = false;
      }
      flushConfig(); // The callback may have scheduled application data.
      if (storageBusy()) return;
    }
    // Storage completion (and onConfigSaved) precedes the actual restart.
    ESP.restart(); return;
  }

#if ARDPORTAL_ENABLE_MQTT
  _mqttClient.startTrial();
#endif
  ArdCooperativeBudget budget;uint32_t now = millis();
  if (_configurationReady && !otaActive() && !_rebootPending) { serviceWifi(now); serviceScan(); }
  budget.checkpoint();serviceHttp(now);
  budget.checkpoint();
#if ARDPORTAL_SUPPORT_WEBSOCKET
  serviceWebSocket(millis());
#endif
#if ARDPORTAL_ENABLE_MQTT
  if (_configurationReady && !otaActive() && !_rebootPending && !_saveQueued) _mqttClient.serviceMqtt(millis());
#endif
  budget.checkpoint();
#if ARDPORTAL_ENABLE_HA
  _homeAssistant.serviceDiscovery(millis());
#elif ARDPORTAL_ENABLE_MQTT && ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _appControls.serviceMqttValues(millis());
#endif
  budget.checkpoint();
  // Yield after discovery temporaries have left the stack; never drain MQTT in a loop.
#if ARDPORTAL_ENABLE_MQTT && ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _appControls.yieldAfterPublish();
#endif

#if ARDPORTAL_ENABLE_MQTT
  _mqttClient.serviceTrial();
#endif

}
void ArdPortal::responseHeader(int code, const char* type, size_t length, bool gzip, bool utf8) {
  // Build every ordinary response in one buffer, without chained temporaries.
  _response = F("HTTP/1.1 "); _response += String(code);
  _response += code == 200 ? F(" OK\r\n") : F(" Response\r\n");
  _response += F("Connection: close\r\nCache-Control: no-store\r\n");
  if (gzip) _response += F("Content-Encoding: gzip\r\n");
  _response += F("Content-Type: "); _response += type;
  if (utf8) _response += F("; charset=utf-8");
  _response += F("\r\nContent-Length: "); _response += String(length);
  _response += F("\r\n\r\n");
  _responseOffset = 0; _httpSince = millis();
}
void ArdPortal::reply(int code, const char* type, const String& body) {
  responseHeader(code, type, body.length()); _response += body;
}
void ArdPortal::replyMessage(int code, ArdUILanguageData::Key key) {
  reply(code,"text/plain",ArdUILanguage::text(key));
}
size_t ArdPortal::beginAsset(const ArdAssetChunk* chunks,size_t count) {
  _assetChunks=chunks;_assetCount=count;_assetIndex=0;
  ArdAssetChunk first=ardAssetChunk(chunks,0);_page=first.data;_pageLength=first.size;_pageOffset=0;
  _assetFooterOffset=8;
  size_t size=ardAssetSize(chunks,count);
  if(first.plainSize==UINT32_MAX) {
    uint32_t crc=0,length=0;ArdCooperativeBudget budget;
    for(size_t i=1;i<count;++i) {
      auto part=ardAssetChunk(chunks,i);
      crc=ardAssetCrcAppend(crc,part.crc,part.plainSize);length+=part.plainSize;
      budget.checkpoint();
    }
    for(unsigned i=0;i<4;++i){_assetFooter[i]=uint8_t(crc>>(8*i));_assetFooter[4+i]=uint8_t(length>>(8*i));}
    _assetFooterOffset=0;size+=8;
  }
  return size;
}
void ArdPortal::closeHttp() {
#if ARDPORTAL_ENABLE_OTA
  if (otaActive()) _ota.abortUpgrade();
#endif
  stopClient(_http); _http = WiFiClient(); _request = String(); _response = String();
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _httpDynamicText=String();
  _httpDynamicPages = _httpDynamicPageStarted = _httpPortalTail = false; _httpDynamicPageIndex = _httpDynamicPageCount = _httpDynamicPageOffset = _httpDynamicPageLength = 0;
#endif
  _httpConfigUpload = false; _httpWaitingStorage = false;
#if ARDPORTAL_SUPPORT_WEBSOCKET
  _wsUpgrade = false;
#endif
  _assetChunks=nullptr;_assetCount=_assetIndex=0;_assetFooterOffset=8;
  _page = nullptr; _pageLength = _pageOffset = _responseOffset = 0;
}
void ArdPortal::serviceHttp(uint32_t now) {
  _httpRequestReady=false;
  serviceHttpIo(now);
  // Release receive buffers/locals from the stack before invoking route callbacks.
  if(_httpRequestReady) {_httpRequestReady=false;handleHttp();}
}
void ArdPortal::serviceHttpIo(uint32_t now) {
  if (!_http) {
    closeHttp();
    _http = _server.accept();
    if (!_http) return;
    _http.setTimeout(100); _request = String(); _httpSince = now;
  }
  if (uint32_t(now - _httpSince) >= (_httpWaitingStorage ? 30000U : (otaActive() ? 15000U : 5000U))) { closeHttp(); return; }
  if (_response.length()) {
    // One bounded write per loop; ESP32 uses send(MSG_DONTWAIT).
    uint8_t chunk[256]; size_t length;
    if (_responseOffset < _response.length()) {
      length = _response.length() - _responseOffset; if (length > sizeof(chunk)) length = sizeof(chunk);
      memcpy(chunk, _response.c_str() + _responseOffset, length);
      size_t written=writeChunk(_http, chunk, length);
      _responseOffset += written;if(written) _httpSince=millis();
    } else if (_page && _pageOffset < _pageLength) {
      length = _pageLength - _pageOffset; if (length > sizeof(chunk)) length = sizeof(chunk);
      memcpy_P(chunk, _page + _pageOffset, length);size_t written=writeChunk(_http, chunk, length);
      _pageOffset += written;if(written) _httpSince=millis();
    } else if(_assetChunks && _assetIndex+1<_assetCount) {
      ArdAssetChunk next=ardAssetChunk(_assetChunks,++_assetIndex);_page=next.data;_pageLength=next.size;_pageOffset=0;
    } else if(_assetFooterOffset<8) {
      size_t written=writeChunk(_http,_assetFooter+_assetFooterOffset,8-_assetFooterOffset);
      _assetFooterOffset+=written;if(written)_httpSince=millis();

#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
    } else if(_httpDynamicPages) {
      if(!_httpDynamicPageStarted) {_response="[";_httpDynamicPageStarted=true;}
      else if(_httpDynamicPageIndex<_httpDynamicPageCount) {
        String part;
        if(_httpPortalTail){const String& menu=_dynamic.pages.menu(_httpDynamicPageIndex);_httpDynamicPageLength=menu.length();part=menu.substring(_httpDynamicPageOffset,_httpDynamicPageOffset+256);}
        else {
          if(!_httpDynamicPageOffset){
            ArdJSON::Limits limits;limits.maxNodes=4096;
            const auto page=_dynamic.pages[_httpDynamicPageIndex];
            if(!page.isValid()||page.isUndefined()){closeHttp();return;}
            _httpDynamicText=ArdJSON::JSON.stringify(page,false,nullptr,limits);
            _httpDynamicPageLength=_httpDynamicText.length();
          }
          part=_httpDynamicText.substring(_httpDynamicPageOffset,_httpDynamicPageOffset+256);
        }

        if(!part.length()) {closeHttp();return;}
        _response=(_httpDynamicPageIndex&&!_httpDynamicPageOffset?String(","):String())+part;
        _httpDynamicPageOffset+=part.length();
        if(_httpDynamicPageOffset==_httpDynamicPageLength) {++_httpDynamicPageIndex;_httpDynamicPageOffset=0;_httpDynamicText=String();}
      } else {_response="]";_httpDynamicPages=false;}
      _responseOffset=0;
    } else if(_httpPortalTail) {
      _page=PORTAL_HTML;_pageLength=strlen_P(_page);_pageOffset=0;_httpPortalTail=false;

#endif
    } else {
#if ARDPORTAL_SUPPORT_WEBSOCKET
      if (_wsUpgrade) {
        WebSocketState& ws=upgradeWebSocket();
        ws.client = _http; _http = WiFiClient(); _wsUpgrade = false;
        _request = String(); _response = String(); _responseOffset = 0;
#if ARDPORTAL_ENABLE_CONSOLE
        ws.cursor = _consoleId > ConsoleCapacity ? _consoleId - ConsoleCapacity : 0;
#endif
        ws.appSent = false; ws.statusSent = false; ws.rxSize = 0; ws.since = ws.pingSince = millis(); ws.pingPending = ws.closing = ws.pongPending = false; return;
      }
#endif
#if defined(ESP8266)
      // Give queued bytes a bounded opportunity to be acknowledged before AP changes.
      if (!_http.flush(1)) return;
      _http.stop(1);
#endif
      closeHttp();
    }
    return;
  }
  if (_httpWaitingStorage) return;
#if ARDPORTAL_ENABLE_OTA
  if (otaActive()) {
    _ota.receiveUpload();
    return;
  }
#endif
  for (size_t budget = 0; budget < 256 && _http.available(); ++budget) {
    int c = _http.read(); if (c < 0) break; _request += char(c);
    if (_request.length() > 16384) { replyMessage(413,ArdUILanguage::Key::s_109); return; }
  }
  int split = _request.indexOf("\r\n\r\n");
  if (split < 0) {
    if (_request.length() > 1536) replyMessage(431,ArdUILanguage::Key::s_109);
    return;
  }
  if (split > 1536) { replyMessage(431,ArdUILanguage::Key::s_109); return; }
  String headers = _request.substring(0, split); headers.toLowerCase();
  int languagePosition = headers.indexOf("\r\nx-ardui-language:");
  if (languagePosition >= 0) {
    int end = headers.indexOf("\r\n", languagePosition + 2); if (end < 0) end = split;
    String code = _request.substring(languagePosition + strlen("\r\nx-ardui-language:"), end); code.trim(); ArdUILanguage::select(code);
  }
  if (_request.indexOf("POST /api/config ") == 0 && !_httpConfigUpload) {
    _httpConfigUpload = true;
    // Release TLS buffers before receiving/parsing the CA form on low-memory ESP8266.
  
#if ARDPORTAL_ENABLE_MQTT
  if (_mqttClient.tlsActive()) closeMqtt();
#endif
  }
  if (headers.indexOf("\r\ntransfer-encoding:") >= 0) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
  uint32_t bodySize = 0;
  int pos = headers.indexOf("\r\ncontent-length:");
  if (pos >= 0) {
    int end = headers.indexOf("\r\n", pos + 2); if (end < 0) end = headers.length();
    String text = headers.substring(pos + 17, end); text.trim();
    if (!number(text, bodySize) || headers.indexOf("\r\ncontent-length:", pos + 2) >= 0) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
  }
  if (_request.indexOf("POST /api/upgrade ") == 0) {
#if ARDPORTAL_ENABLE_OTA
    _ota.startUpload(bodySize, split);
#else
    replyMessage(404,ArdUILanguage::Key::s_131);
#endif
    return;
  }
  if (bodySize > 14336) { replyMessage(413,ArdUILanguage::Key::s_109); return; }
  if (_request.length() >= size_t(split + 4) + bodySize) {
    _request.remove(split + 4 + bodySize); // Ignore pipelined bytes; every response closes the connection.
    _httpRequestReady=true;
  }
}
void ArdPortal::handleHttp() {
  int firstSpace = _request.indexOf(' '), secondSpace = _request.indexOf(' ', firstSpace + 1);
  if (firstSpace < 0 || secondSpace < 0) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
  String method = _request.substring(0, firstSpace), path = _request.substring(firstSpace + 1, secondSpace);
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if(dynamicHttp(method,path))return;
#endif
  if(method=="GET"&&path=="/api/info") {reply(200,"application/json",infoJson());return;}
  handleBuiltinHttp(method,path);
}
void ArdPortal::handleBuiltinHttp(const String& method,const String& path) {
#if !ARDPORTAL_ENABLE_OTA
  if(path=="/upgrade" || path=="/api/upgrade") { replyMessage(404,ArdUILanguage::Key::s_131); return; }
#endif
#if !ARDPORTAL_ENABLE_CONSOLE
  if(path=="/console" || path=="/api/console") { replyMessage(404,ArdUILanguage::Key::s_131); return; }
#endif
#if !ARDPORTAL_ENABLE_MQTT
  if(path=="/mqtt" || path=="/api/mqtt/connect") { replyMessage(404,ArdUILanguage::Key::s_131); return; }
#endif
  if(method=="GET"&&(path=="/portal.css"||path=="/portal.js")) {
    bool css=path=="/portal.css";size_t length=css?beginAsset(PORTAL_CSS_GZIP,sizeof(PORTAL_CSS_GZIP)/sizeof(PORTAL_CSS_GZIP[0])):beginAsset(PORTAL_JS_GZIP,sizeof(PORTAL_JS_GZIP)/sizeof(PORTAL_JS_GZIP[0]));
    responseHeader(200,css?"text/css":"application/javascript",length,true,true);return;
  }
  if (method == "GET" && path == "/api/languages") {
    _page = ArdUILanguageData::Manifest; _pageLength = strlen_P(_page); _pageOffset = 0;
    responseHeader(200,"application/json",_pageLength,false,true); return;
  }
  if (method == "GET" && path.indexOf("/api/language?code=") == 0) {
    String code = path.substring(strlen("/api/language?code=")); int index = ArdUILanguage::find(code);
    if (index < 0) { replyMessage(404,ArdUILanguage::Key::s_131); return; }
    ArdUILanguage::select(code);
    auto language = ArdUILanguage::language(size_t(index));
    size_t length=beginAsset(language.json,language.count);
    responseHeader(200,"application/json",length,true,true); return;
  }
#if ARDPORTAL_SUPPORT_WEBSOCKET
  if (method == "GET" && (path == "/api/events"
#if ARDPORTAL_ENABLE_CONSOLE
    || path == "/api/console"
#endif
    )) {
    _wsUpgradeConsole=path=="/api/console";
    if(upgradeWebSocket().client){replyMessage(409,ArdUILanguage::Key::s_132);return;}
    String key, upgrade, connection, version;
    int offset = _request.indexOf("\r\n") + 2;
    while (offset > 1) {
      int end = _request.indexOf("\r\n", offset); if (end <= offset) break;
      String line = _request.substring(offset, end); int colon = line.indexOf(':');
      if (colon > 0) {
        String name = line.substring(0, colon); name.toLowerCase();
        String value = line.substring(colon+1); value.trim();
        if (name == "sec-websocket-key") key = value;
        if (name == "sec-websocket-version") version = value;
        if (name == "upgrade") upgrade = value;
        if (name == "connection") connection = value;
      }
      offset = end+2;
    }
    upgrade.toLowerCase(); connection.toLowerCase();
    bool validKey = key.length() == 24 && key[22] == '=' && key[23] == '=';
    for(size_t i=0;validKey && i<22;++i) { char c=key[i]; validKey=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='+'||c=='/'; }
    if (!validKey || version != "13" || upgrade != "websocket" || connection.indexOf("upgrade") < 0) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
    _response = String(F("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ")) + ArdPortalWS::accept(key) + "\r\n\r\n";
    _responseOffset=0; _httpSince=millis(); _wsUpgrade=true; return;
  }
#endif
  if (_apActive && method == "GET" && path != "/" && path != "/device" && path != "/wifi" &&
      path.indexOf("/p/") != 0 && path != "/ap" && path != "/mqtt" && path != "/upgrade" && path != "/console" && path.indexOf("/api/") != 0) {
    _response = String(F("HTTP/1.1 302 Found\r\nConnection: close\r\nCache-Control: no-store\r\nLocation: http://")) + apIP().toString() + "/\r\nContent-Length: 0\r\n\r\n";
    _responseOffset = 0; _httpSince = millis(); return;
  }
  if (method == "GET" && (path.indexOf("/api/") != 0 && path.indexOf('.') < 0)) {
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
    const size_t catalogLength=_dynamic.pages.menuLength();
    _page=PORTAL_PAGE_HEAD;_pageLength=strlen_P(_page);_pageOffset=0;
    _httpDynamicPages=true;_httpDynamicPageStarted=false;_httpPortalTail=true;_httpDynamicPageIndex=0;_httpDynamicPageOffset=0;_httpDynamicPageCount=_dynamic.pages.length();
    size_t length=_pageLength+catalogLength+strlen_P(PORTAL_HTML);
#else
    size_t length=beginAsset(PORTAL_HTML_GZIP,sizeof(PORTAL_HTML_GZIP)/sizeof(PORTAL_HTML_GZIP[0]));
#endif
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
    responseHeader(200,"text/html",length,false,true);
#else
    responseHeader(200,"text/html",length,true,true);
#endif
    return;
  }
  if (method == "POST" && path == "/api/restart") {
    if (!_configurationReady || mqttTrialActive() || _rebootPending) {replyMessage(409,ArdUILanguage::Key::s_132);return;}
    if(_request.substring(_request.indexOf("\r\n\r\n")+4)!="confirm=RESTART") {replyMessage(400,ArdUILanguage::Key::s_124);return;}
    replyMessage(202,ArdUILanguage::Key::s_179);scheduleRestart(RestartReason::Portal);return;
  }
  if (method == "POST" && path == "/api/factory-reset") {
    if (!_configurationReady || storageBusy() || mqttTrialActive() || _rebootPending) { replyMessage(409,ArdUILanguage::Key::s_132); return; }
    if (_request.substring(_request.indexOf("\r\n\r\n") + 4) != "confirm=RESET") { replyMessage(400,ArdUILanguage::Key::s_124); return; }
    Config defaults; defaults.deviceName = defaultDeviceName(); defaults.apName = defaults.deviceName;
    closeMqtt();
    if (!scheduleConfig(defaults, ArdJSON::JSONVar::object(), ChangeSource::FactoryReset, true)) { replyMessage(500,ArdUILanguage::Key::s_107); return; }
    _resetAfterSave = true; _httpWaitingStorage = true; return;
  }
  if (method == "POST" && path == "/api/storage/format") {
    if (!_configurationReady || storageBusy() || mqttTrialActive() || _rebootPending) { replyMessage(409,ArdUILanguage::Key::s_132); return; }
    if (_request.substring(_request.indexOf("\r\n\r\n") + 4) != "confirm=FORMAT") { replyMessage(400,ArdUILanguage::Key::s_124); return; }
    closeMqtt(); _httpWaitingStorage = true;
    if (!_storage.format([this](const ArdFS::Result& result) {
      _storageMounted = _storage.mounted(); _storageOK = false; _storageError = result.error;
      bool respond = _httpWaitingStorage; _httpWaitingStorage = false;
      if (respond) reply(result.ok ? 200 : 500, "text/plain", result.ok ? ArdUILanguage::text(ArdUILanguage::Key::s_128) : result.error);
      if (result.ok) scheduleRestart(RestartReason::StorageFormat);
    })) { _httpWaitingStorage = false; replyMessage(500,ArdUILanguage::Key::s_129); }
    return;
  }
  if (method == "GET" && path == "/api/status") {
    String out;
    out += F("{\"wifi\":"); out += wifiConnected() ? "true" : "false"; out += F(",\"mqtt\":"); out += mqttConnected() ? "true" : "false";
    out += F(",\"state\":"); out += String(int(_wifiState)); out += F(",\"wifiRevision\":"); out += String(_wifiRevision); out += F(",\"wifiResult\":"); out += String(_wifiResult); out += F(",\"mqttRevision\":"); out += String(mqttTrialRevision()); out += F(",\"mqttResult\":"); out += String(mqttTrialResult()); out += F(",\"mqttResultMessage\":"); appendQuoted(out,mqttTrialMessage()); out += F(",\"mqttState\":"); out += String(int(mqttState()));
    out += F(",\"ap\":"); out += _apActive ? "true" : "false"; out += F(",\"ip\":"); appendQuoted(out,localIP().toString());
    out += F(",\"apIp\":"); appendQuoted(out,apIP().toString()); out += F(",\"storage\":"); out += _storageOK ? "true" : "false"; out += F(",\"tls\":"); out += _config.mqttTls ? "true" : "false";
    out += F(",\"clockReady\":"); out += tlsClockReady() ? "true" : "false"; out += F(",\"filesystemMounted\":"); out += _storageMounted ? "true" : "false";
    out += F(",\"configurationReady\":"); out += _configurationReady ? "true" : "false"; out += F(",\"storageBusy\":"); out += storageBusy() ? "true" : "false"; out += F(",\"storageError\":"); appendQuoted(out,_storageError); out += F(",\"scanCompletedId\":"); out += String(_scanCompletedId); out += F(",\"deviceName\":"); appendQuoted(out,_config.deviceName); out += "}";
    reply(200, "application/json", out); return;
  }
  if (method == "GET" && path == "/api/config") {
    String out;
    out += F("{\"ssid\":"); appendQuoted(out,_config.ssid);
    out += F(",\"host\":"); appendQuoted(out,_config.host);
    out += F(",\"port\":"); out += String(_config.port);
    out += F(",\"user\":"); appendQuoted(out,_config.user);
    out += F(",\"mqttTls\":"); out += String(_config.mqttTls ? "1" : "0");
    out += F(",\"caCert\":"); appendQuoted(out,_config.caCert);
    out += F(",\"apName\":"); appendQuoted(out,_config.apName);
    out += F(",\"deviceName\":"); appendQuoted(out,_config.deviceName);
    out += F(",\"mqttName\":"); appendQuoted(out,ArdDeviceName::mqtt(_config.deviceName));
    out += F(",\"deviceDescription\":"); appendQuoted(out,_config.deviceDescription);
    out += '}';
    reply(200,"application/json",out);return;
  }
  if (method == "POST" && path == "/api/scan") {
    if (_scanRequested || _scanning) { replyMessage(409,ArdUILanguage::Key::s_132); return; }
    _scanRequested = true; ++_scanId;
    reply(202, "application/json", String(F("{\"scanId\":")) + String(_scanId) + "}"); return;
  }
  if (method == "GET" && path == "/api/networks") { reply(200, "application/json", _networks); return; }
  if (method != "POST" || (path != "/api/config" && path != "/api/mqtt/connect")) { replyMessage(404,ArdUILanguage::Key::s_131); return; }
  if (!_configurationReady || storageBusy() || mqttTrialActive()) { replyMessage(409,ArdUILanguage::Key::s_132); return; }
  const char* keys[] = {"ssid", "password", "host", "port", "user", "mqttPassword", "mqttTls", "caCert", "apName", "apPassword", "deviceName", "clearWifi", "deviceDescription"};
  String values[13]; uint16_t seen = 0;
  String body = _request.substring(_request.indexOf("\r\n\r\n") + 4);
  int offset = 0;
  while (offset < int(body.length())) {
    int end = body.indexOf('&', offset); if (end < 0) end = body.length();
    String pair = body.substring(offset, end); int equal = pair.indexOf('=');
    if (equal < 0) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
    String key, value;
    if (!decode(pair.substring(0, equal), key) || !decode(pair.substring(equal + 1), value)) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
    int index = -1;
    for (int i = 0; i < 13; ++i) if (key == keys[i]) index = i;
    if (index < 0 || (seen & (1 << index))) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
    seen |= 1 << index; values[index] = value; offset = end + 1;
  }
#if !ARDPORTAL_ENABLE_MQTT
  if(seen & 252) { replyMessage(404,ArdUILanguage::Key::s_131); return; }
#endif
  if (!seen) { replyMessage(400,ArdUILanguage::Key::s_120); return; }
  Config c = _config;
  if (seen & 1) c.ssid = values[0];
  if (seen & 2) c.password = values[1].length() || c.ssid != _config.ssid ? values[1] : _config.password;
  else if (c.ssid != _config.ssid) c.password = "";
  if (seen & 4) c.host = values[2];
  if (seen & 8) {
    uint32_t port;
    if (!number(values[3], port) || port < 1 || port > 65535) { replyMessage(400,ArdUILanguage::Key::s_142); return; }
    c.port = port;
  }
  if (seen & 16) c.user = values[4];
  if (seen & 32) c.mqttPassword = values[5].length() || c.host != _config.host || c.user != _config.user ? values[5] : _config.mqttPassword;
  else if (c.host != _config.host || c.user != _config.user) c.mqttPassword = "";
  if (seen & 64) {
    if (values[6] != "0" && values[6] != "1") { replyMessage(400,ArdUILanguage::Key::s_142); return; }
#if !ARDPORTAL_ENABLE_MQTT_TLS
    if(values[6]=="1") {replyMessage(400,ArdUILanguage::Key::s_211);return;}
#endif
    c.mqttTls = values[6] == "1";
  }
  if (seen & 128) c.caCert = values[7];
#if !ARDPORTAL_ENABLE_MQTT_TLS
  if((seen&64)&&!c.mqttTls)c.caCert=String();
#endif
#if !ARDPORTAL_ENABLE_MQTT_TLS
  if(path=="/api/mqtt/connect") {c.mqttTls=false;c.caCert=String();}
#endif
  if (seen & 256) c.apName = values[8];
  if ((seen & 512) && values[9].length()) c.apPassword = values[9];
  if (seen & 1024) {
    c.deviceName = values[10];
    if (!c.deviceName.length()) { replyMessage(400,ArdUILanguage::Key::s_142); return; }
  }
  if (seen & 4096) c.deviceDescription = values[12];
  if (seen & 2048) {
    if (values[11] != "1" || seen != 2048) { replyMessage(400,ArdUILanguage::Key::s_142); return; }
    c.ssid = ""; c.password = "";
  }
  if ((seen & 256) && values[8] != ArdDeviceName::ap(c.deviceName)) { replyMessage(400,ArdUILanguage::Key::s_142); return; }
  c.apName = ArdDeviceName::ap(c.deviceName);
  if (!validConfig(c)) { replyMessage(400,ArdUILanguage::Key::s_142); return; }
#if ARDPORTAL_ENABLE_MQTT
  if (path == "/api/mqtt/connect" && ((seen & ~252) || !(seen & 4))) { replyMessage(400,ArdUILanguage::Key::s_142); return; }
  if (path == "/api/mqtt/connect" && c.host.length()) {
    if (!wifiConnected()) { replyMessage(409,ArdUILanguage::Key::s_144); return; }
    _mqttClient.prepareTrial(std::move(c)); _request = String();
    replyMessage(202,ArdUILanguage::Key::s_145); return;
  }
#endif
  if (!scheduleConfig(c, _appConfig, ChangeSource::Portal, true)) { replyMessage(500,ArdUILanguage::Key::s_107); return; }
  _wifiConnectAfterSave = (seen & 3) != 0;
  _httpWaitingStorage = true; _request = String();

}

String ArdPortal::mqttTopic(const char* kind, const char* command) const {
#if ARDPORTAL_ENABLE_MQTT
 return _mqttClient.mqttTopic(kind, command); 
#else
  (void)kind; (void)command; return String();
#endif
}
bool ArdPortal::validMqttTopic(const String& topic, bool subscription) const {
#if ARDPORTAL_ENABLE_MQTT
 return _mqttClient.validMqttTopic(topic, subscription); 
#else
  (void)topic; (void)subscription; return false;
#endif
}
bool ArdPortal::publish(const char* topic, const char* payload, bool retain) {
#if ARDPORTAL_ENABLE_MQTT
 return _mqttClient.publish(topic, payload, retain); 
#else
  (void)topic; (void)payload; (void)retain; return false;
#endif
}

bool ArdPortal::subscribe(const char* topic) {
#if ARDPORTAL_ENABLE_MQTT
 return _mqttClient.subscribe(topic); 
#else
  (void)topic; return false;
#endif
}

String ArdPortal::chipId() {
  char id[17];
#if defined(ESP32)
  snprintf(id, sizeof(id), "%012llX", static_cast<unsigned long long>(ESP.getEfuseMac()));
#else
  snprintf(id, sizeof(id), "%06X", ESP.getChipId());
#endif
  return id;
}
void ArdPortal::configureIdentity() {

#if ARDPORTAL_ENABLE_MQTT
  _mqttClient.configureIdentity(ArdDeviceName::mqtt(_config.deviceName));
#endif
#if defined(ESP32)
  WiFi.setHostname(ArdDeviceName::hostname(_config.deviceName,defaultDeviceName()).c_str());
#else
  WiFi.hostname(ArdDeviceName::hostname(_config.deviceName,defaultDeviceName()).c_str());
#endif
}
String ArdPortal::infoJson() {
  time_t current = time(nullptr); char timestamp[32] = "";
  if (tlsClockReady()) utcTimestamp(current, timestamp, sizeof(timestamp));
#if defined(ESP32)
  String chip = ESP.getChipModel(); uint32_t heap = ESP.getHeapSize(); uint32_t physicalFlash = ESP.getFlashChipSize();uint32_t maximumBlock=ESP.getMaxAllocHeap();
#else
  String chip = "ESP8266"; uint32_t heap = 0; uint32_t physicalFlash = ESP.getFlashChipRealSize();uint32_t maximumBlock=ESP.getMaxFreeBlockSize();
#endif
  String out;
  out += F("{\"deviceName\":"); appendQuoted(out,_config.deviceName); out += F(",\"deviceDescription\":"); appendQuoted(out,_config.deviceDescription); out += F(",\"deviceManufacturer\":"); appendQuoted(out,_config.deviceManufacturer); out += F(",\"chip\":"); appendQuoted(out,chip);
  out += F(",\"chipId\":"); appendQuoted(out,chipId()); out += F(",\"flashBytes\":"); out += String(ESP.getFlashChipSize());
  out += F(",\"physicalFlashBytes\":"); out += String(physicalFlash); out += F(",\"heapBytes\":"); out += String(heap); out += F(",\"freeHeapBytes\":"); out += String(ESP.getFreeHeap());
  out += F(",\"resetReason\":"); appendQuoted(out,ArdUILanguage::text(resetReasonKey())); out += F(",\"minimumFreeHeapBytes\":"); out += String(_minimumFreeHeap==UINT32_MAX?ESP.getFreeHeap():_minimumFreeHeap); out += F(",\"maximumHeapBlockBytes\":"); out += String(maximumBlock);
  out += F(",\"sketchBytes\":"); out += String(ESP.getSketchSize()); out += F(",\"otaFreeBytes\":"); out += String(ESP.getFreeSketchSpace());
  out += F(",\"uptimeSeconds\":"); out += String(static_cast<unsigned long>(_uptimeMs / 1000)); out += F(",\"utc\":"); appendQuoted(out,timestamp);
  out += F(",\"wifi\":"); out += wifiConnected() ? "true" : "false"; out += F(",\"mqtt\":"); out += mqttConnected() ? "true" : "false";
  out += F(",\"wifiState\":"); out += String(int(_wifiState)); out += F(",\"mqttState\":"); out += String(int(mqttState()));
  out += F(",\"ip\":"); appendQuoted(out,localIP().toString()); out += F(",\"apIp\":"); appendQuoted(out,apIP().toString());
  out += F(",\"storageBackend\":"); appendQuoted(out,"ArdFS"); out += F(",\"jsonBackend\":"); appendQuoted(out,"ArdJSON");
  out += F(",\"filesystemMounted\":"); out += _storageMounted ? "true" : "false"; out += F(",\"configurationReady\":"); out += _configurationReady ? "true" : "false"; out += F(",\"storageBusy\":"); out += storageBusy() ? "true" : "false"; out += F(",\"storageError\":"); appendQuoted(out,_storageError); out += "}";
  return out;
}

#if ARDPORTAL_ENABLE_CONSOLE
void ArdPortal::consoleLine(bool mqtt, const String& text) {
  if(!mqtt && !ARDPORTAL_ENABLE_CONSOLE_MESSAGES) return;
  if(ESP.getFreeHeap()<24576) _consoleHistoryBudget=2048;
  ConsoleLine& line = _console[_consoleId % ConsoleCapacity];line=ConsoleLine();
  size_t used=0;for(const auto& entry:_console) used+=entry.text.length();
  size_t incoming=(mqtt?text.length():(text.length()>320?320:text.length()))+64;
  for(size_t i=1;used+incoming>_consoleHistoryBudget&&i<ConsoleCapacity;++i) {auto& old=_console[(_consoleId+i)%ConsoleCapacity];used-=old.text.length();old=ConsoleLine();}
  line.id = ++_consoleId; line.mqtt = mqtt;
  String timestamp = String(millis()) + " ms";
  if (tlsClockReady()) {
    time_t current = time(nullptr); char formatted[32];
    if (utcTimestamp(current, formatted, sizeof(formatted))) timestamp = formatted;
  }
  line.text = "[" + timestamp + "] " + (mqtt?text:text.substring(0,320));
  size_t total=0;for(const auto& entry:_console) total+=entry.text.length();
  for(size_t i=1;total>_consoleHistoryBudget&&i<ConsoleCapacity;++i) {auto& old=_console[(_consoleId-1+i)%ConsoleCapacity];total-=old.text.length();old=ConsoleLine();}
}
#else
void ArdPortal::consoleLine(bool , const String& ) {}
#endif
#if ARDPORTAL_ENABLE_CONSOLE_MESSAGES
void ArdPortal::log(const String& message) { consoleLine(false, message); }
#else
void ArdPortal::log(const String& ) {}
#endif
#if ARDPORTAL_ENABLE_CONSOLE
void ArdPortal::logMqtt(const String& direction, const String& topic, const uint8_t* payload, size_t length) {
  String value; bool printable=true;
  for(size_t i=0;i<length;++i) if(payload[i]==0) {printable=false;break;}
  if(!printable) value=ArdUILanguage::text(ArdUILanguage::Key::s_178)+String(length);
  else for(size_t i=0;i<length;++i) value+=char(payload[i]);
  consoleLine(true,direction+" "+topic+" = "+value);
}
#else
void ArdPortal::logMqtt(const String& , const String& , const uint8_t* , size_t ) {}
#endif
#if ARDPORTAL_SUPPORT_WEBSOCKET
ArdPortal::WebSocketState& ArdPortal::upgradeWebSocket() {
#if ARDPORTAL_ENABLE_CONSOLE
  if(_wsUpgradeConsole)return _consoleWs;
#endif
  return _eventWs;
}
void ArdPortal::serviceWebSocket(uint32_t now) {
  serviceWebSocket(_eventWs,now);
#if ARDPORTAL_ENABLE_CONSOLE
  serviceWebSocket(_consoleWs,now);
#endif
}
void ArdPortal::closeWebSocket(WebSocketState& ws) {
  stopClient(ws.client); ws.client = WiFiClient(); ws.tx=String(); ws.txOffset=0; ws.rxSize=0; ws.appSent=false; ws.statusSent=false; ws.closing=false; ws.pingPending=false; ws.pongPending=false; ws.pong=String();
}
void ArdPortal::queueWebSocket(WebSocketState& ws,uint8_t opcode, const String& payload) {
  ws.tx=""; ws.tx+=char(0x80|opcode);
  if(payload.length()<126) ws.tx+=char(payload.length());
  else { ws.tx+=char(126); ws.tx+=char(payload.length()>>8); ws.tx+=char(payload.length()&255); }
  ws.tx+=payload; ws.txOffset=0; ws.since=millis();
}
#if ARDPORTAL_ENABLE_CONSOLE
void ArdPortal::websocketCommand(const String& command) {
  String topic, value; unsigned seen=0; int offset=0;
  while(offset<int(command.length())) {
    int end=command.indexOf('&',offset);if(end<0)end=command.length();
    String pair=command.substring(offset,end);int equal=pair.indexOf('=');String key, decoded;
    if(equal<0 || !decode(pair.substring(0,equal),key) || !decode(pair.substring(equal+1),decoded)) { log(ArdUILanguage::text(ArdUILanguage::Key::s_120)); return; }
    unsigned flag=key=="topic"?1:key=="value"?2:0;
    if(!flag || (seen&flag)) { log(ArdUILanguage::text(ArdUILanguage::Key::s_120)); return; }
    seen|=flag; if(flag==1)topic=decoded;else value=decoded;offset=end+1;
  }
  String error;
  if(seen!=3) error=ArdUILanguage::text(ArdUILanguage::Key::s_120);
  else if(!topic.length() || topic.indexOf('#')>=0 || topic.indexOf('+')>=0) error=ArdUILanguage::text(ArdUILanguage::Key::s_120);
  else if(mqttTrialActive()) error=ArdUILanguage::text(ArdUILanguage::Key::s_132);
  else if(!mqttConnected()) error=ArdUILanguage::text(ArdUILanguage::Key::s_147);
  else if(topic.length()+value.length()+2>ArdMqtt::PacketCapacity-5) error=ArdUILanguage::text(ArdUILanguage::Key::s_164);
  else if(!_mqttClient.publishRaw(topic.c_str(),value.c_str())) error=ArdUILanguage::text(ArdUILanguage::Key::s_132);
  if(error.length()) { log(error); consoleLine(true,error); }
}
#else
void ArdPortal::websocketCommand(const String&) {}
#endif
void ArdPortal::serviceWebSocket(WebSocketState& ws,uint32_t now) {
  if(!ws.client) { if(ws.tx.length() || ws.rxSize)closeWebSocket(ws); return; }
  if((ws.tx.length() && uint32_t(now-ws.since)>=5000) || (ws.rxSize && uint32_t(now-ws.frameSince)>=5000)) { closeWebSocket(ws); return; }
  if(ws.pingPending && uint32_t(now-ws.pingSince)>=15000) {closeWebSocket(ws);return;}
  // Bounded receive and transmit; browser frames must be masked and unfragmented.
  for(size_t budget=0;budget<256 && ws.client.available();++budget) {
    if(ws.rxSize==ws.capacity) { closeWebSocket(ws);return; }
    int c=ws.client.read();if(c<0)break;if(!ws.rxSize)ws.frameSince=now;ws.rx[ws.rxSize++]=c;
    if(ws.rxSize<2)continue;
    uint8_t opcode=ws.rx[0]&15;size_t length=ws.rx[1]&127,header=2;
    if((ws.rx[0]&0x70) || !(ws.rx[0]&0x80) || !(ws.rx[1]&0x80) || length==127 || (opcode!=1 && opcode!=8 && opcode!=9 && opcode!=10)) { closeWebSocket(ws);return; }
    if(length==126) { if(ws.rxSize<4)continue;length=(size_t(ws.rx[2])<<8)|ws.rx[3];header=4;if(length<126){closeWebSocket(ws);return;} }
    if(length>1024 || (opcode>=8 && length>125)) { closeWebSocket(ws);return; }
    if(ws.rxSize<header+4+length)continue;
    String payload;payload.reserve(length);
    for(size_t i=0;i<length;++i)payload+=char(ws.rx[header+4+i]^ws.rx[header+(i%4)]);
    ws.rxSize=0;
    if(opcode==8){if(length==1 || ws.txOffset){closeWebSocket(ws);return;}queueWebSocket(ws,8,payload);ws.closing=true;break;}
    if(opcode==10 && payload=="ArdUI")ws.pingPending=false;
    if(opcode==9){ws.pong=payload;ws.pongPending=true;}
    if(opcode==1){if(!ws.logs){closeWebSocket(ws);return;}websocketCommand(payload);}
  }
  if(!ws.tx.length() && !ws.closing) {
    if(ws.pongPending){queueWebSocket(ws,10,ws.pong);ws.pong="";ws.pongPending=false;}
    else if(!ws.pingPending && uint32_t(now-ws.pingSince)>=20000) {queueWebSocket(ws,9,"ArdUI");ws.pingPending=true;ws.pingSince=now;}
    else if(!ws.statusSent || ws.mqtt!=mqttConnected() || uint32_t(now-ws.statusSince)>=10000) {
      ws.mqtt=mqttConnected();ws.statusSent=true;ws.statusSince=now;
      queueWebSocket(ws,1,String("{\"type\":\"status\",\"mqtt\":")+(ws.mqtt?"true":"false")+"}");
    } else if(!ws.appSent || ws.appRevision!=_appRevision) {
      ws.appSent=true; ws.appRevision=_appRevision;
      queueWebSocket(ws,1,String("{\"type\":\"app\",\"revision\":")+String(ws.appRevision)+"}");
    }
#if ARDPORTAL_ENABLE_CONSOLE
    if(!ws.tx.length() && ws.logs) {
      uint32_t oldest=_consoleId>ConsoleCapacity?_consoleId-ConsoleCapacity:0;
      if(ws.cursor<oldest)ws.cursor=oldest;
      if(ws.cursor<_consoleId) {
        const ConsoleLine& line=_console[ws.cursor%ConsoleCapacity];++ws.cursor;
        if(line.id==ws.cursor&&line.text.length()) queueWebSocket(ws,1,String("{\"type\":\"log\",\"channel\":")+quote(line.mqtt?"mqtt":"messages")+String(F(",\"id\":")) +String(line.id)+String(F(",\"text\":")) +quote(line.text)+"}");
      }
    }
#endif
  }
  if(ws.tx.length()) {
    size_t length=ws.tx.length()-ws.txOffset;if(length>256)length=256;
    ws.txOffset+=writeChunk(ws.client,reinterpret_cast<const uint8_t*>(ws.tx.c_str())+ws.txOffset,length);
    if(ws.txOffset==ws.tx.length()){ws.tx="";ws.txOffset=0;if(ws.closing)closeWebSocket(ws);}
  }
}

#endif
namespace {
ArdJSON::Limits snapshotLimits() {
  ArdJSON::Limits limits; limits.maxInputBytes = ArdFS::MaxBytes;
  limits.maxOutputBytes = ArdFS::MaxBytes; limits.maxStringBytes = 4096;
  limits.maxDepth = 10; limits.maxNodes = 256; return limits;
}
bool validApp(const ArdJSON::JSONVar& value) {
  if (value.type() != ArdJSON::JSONVar::Type::Object || !value.isValid()) return false;
  ArdJSON::Limits limits = snapshotLimits(); limits.maxOutputBytes = 2048;
  return ArdJSON::JSON.measure(value, nullptr, limits) != 0;
}
}
void ArdPortal::finishLoad(const ArdFS::Result& result) {
  bool restored = false;
  _storageMounted = _storage.mounted(); _storageError = result.error;
  if (result.ok && result.found) {
    ArdJSON::JSONVar root = _json.parse(result.data, nullptr, snapshotLimits());
    const ArdJSON::JSONVar& view=root;
    Config loaded;
    if (root.isValid() && root.type() == ArdJSON::JSONVar::Type::Object && root.length() == 2 &&
        view.hasOwnProperty("config") && validApp(view["app"]) &&
        ArdPortalJson::decodeValue(view["config"], loaded)) {
      _config = std::move(loaded); _appConfig = std::move(root["app"]); restored = true;
    } else if (ArdPortalJson::decode(result.data, loaded, _json)) { _config = std::move(loaded); restored = true; }
    else _storageError = ArdUILanguage::text(ArdUILanguage::Key::s_166);
  }
  _storageOK = _storageMounted && result.ok && !_storageError.length();
  if (!_config.deviceName.length()) _config.deviceName = defaultDeviceName();
  _config.apName = ArdDeviceName::ap(_config.deviceName); _apName = _config.apName; _apPassword = _config.apPassword;
  configureIdentity(); _configurationReady = true;
  log(result.recovered ? ArdUILanguage::text(ArdUILanguage::Key::s_167) : ArdUILanguage::text(ArdUILanguage::Key::s_168));
  if (_storageError.length()) log(_storageError);

#if ARDPORTAL_ENABLE_MQTT
  _mqttClient.configurationLoaded();
#endif
  if (_config.ssid.length()) connectWifi(); else startAP();
  if (_readyCallback) _readyCallback(restored);
}
void ArdPortal::serviceStorage(uint32_t now) {
  _storage.loop(); _storageMounted = _storage.mounted();
  if (!_loadStarted && _storage.ready()) {
    _loadStarted = true;
    if (!_storage.read("/ardportal.json", [this](const ArdFS::Result& result) { finishLoad(result); })) {
      ArdFS::Result result; result.error = _storage.error(); finishLoad(result);
    }
  }
  if (!_savePending || _saveQueued || _storage.busy() || !_configurationReady) return;
  if (!_forceSave && (uint32_t(now - _dirtySince) < _options.configSaveDelayMs ||
      (_hasCommitted && uint32_t(now - _lastCommit) < _options.configMinWriteIntervalMs))) return;
  String json;
  {
    ArdJSON::JSONVar root = ArdJSON::JSONVar::object();
    root["config"] = ArdPortalJson::value(_pending);
    root["app"] = nullptr;
    if (root.hasOwnProperty("app")) {
      // Borrow the pending tree during serialization; restore it even on failure.
      root["app"] = std::move(_pendingApp);
      json = _json.stringify(root, false, nullptr, snapshotLimits());
      _pendingApp = std::move(root["app"]);
    }
  }
#if ARDPORTAL_ENABLE_MQTT
  const Config& active = _mqttClient.saveBaseline();
#else
  const Config& active = _config;
#endif
  _saveHasChanges = !ArdPortalJson::equal(active, _pending) ||
    _json.stringify(_appConfig) != _json.stringify(_pendingApp);
  _saveQueued = true;
  if (!json.length() || !_storage.write("/ardportal.json", json, [this](const ArdFS::Result& result) { finishSave(result); })) {
    ArdFS::Result result; result.error = _storage.mounted() ? ArdUILanguage::text(ArdUILanguage::Key::s_169) : ArdUILanguage::text(ArdUILanguage::Key::s_170);
    finishSave(result);
  }
}
bool ArdPortal::scheduleConfig(const Config& config, const ArdJSON::JSONVar& app, ChangeSource source, bool immediate) {
  if (!_configurationReady || _saveQueued || _pendingReady || !_storage.mounted() || otaActive() ||
      (_rebootPending && !(_beforeRestartRunning && source == ChangeSource::Application && ArdPortalJson::equal(config,_config))) ||
      !validConfig(config) || !validApp(app)) return false;
  if (_savePending && (source != ChangeSource::Application || _saveSource != source)) return false;
  _pending = config;
  if (!_pending.deviceName.length()) _pending.deviceName = defaultDeviceName();
  _pending.apName = ArdDeviceName::ap(_pending.deviceName);
  _pendingApp = app; _saveSource = source; _savePending = true; _forceSave = immediate; _dirtySince = millis();
  return true;
}
bool ArdPortal::setDeviceManufacturer(const String& manufacturer) {
  Config config = _savePending ? _pending : _config;
  config.deviceManufacturer = manufacturer;
  return setPortalConfig(config);
}
bool ArdPortal::setDeviceDescription(const String& description) {
  Config config = _savePending ? _pending : _config;
  config.deviceDescription = description;
  return setPortalConfig(config);
}
bool ArdPortal::setPortalConfig(const Config& config) {
#if !ARDPORTAL_ENABLE_MQTT_TLS
  if(config.mqttTls)return false;
#endif
  if (_rebootPending || mqttTrialActive() || _httpWaitingStorage) return false;
  return scheduleConfig(config, _savePending ? _pendingApp : _appConfig, ChangeSource::Application, false);
}
ArdJSON::JSONVar ArdPortal::getAppConfigValue(const char* key) const {
  if (!key) return ArdJSON::JSONVar();
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (_appControls.stateValues().hasOwnProperty(key)) return static_cast<const ArdJSON::JSONVar&>(_appControls.stateValues())[key];
#endif
  const auto& value=static_cast<const ArdJSON::JSONVar&>(_appConfig)[key];
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  const auto& field=_dynamic.field(key);
  if(ArdHa::extended(field)) {ArdJSON::JSONVar resolved=field;if(!ArdHa::normalize(resolved,true)) return ArdJSON::JSONVar();return ArdHa::validValue(resolved,value)?value:resolved["default"];}
  return !field.isUndefined() && (value.isUndefined() || !ArdDynamicPages::validValue(field,value)) ? ArdDynamicPages::initial(field) : value;
#else
  return value;
#endif
}
bool ArdPortal::setAppConfigValue(const char* key, const ArdJSON::JSONVar& value) {
  if (!key || !*key || strlen(key) > 64 || !value.isValid() || value.isUndefined() ||
      (_rebootPending && !_beforeRestartRunning) || mqttTrialActive() || _httpWaitingStorage) return false;
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (ArdHa::extended(_dynamic.field(key)) && !_dynamic.field(key)["persist"].asBool()) return setAppStateValue(key,value);
  if (!_dynamic.field(key).isUndefined() && !ArdDynamicPages::validValue(_dynamic.field(key),value)) return false;
#endif
  ArdJSON::JSONVar app = _savePending ? _pendingApp : _appConfig; app[key] = value;
  return scheduleConfig(_savePending ? _pending : _config, app, ChangeSource::Application, false);
}
bool ArdPortal::removeAppConfigValue(const char* key) {
  if (!key || (_rebootPending && !_beforeRestartRunning) || mqttTrialActive() || _httpWaitingStorage) return false;
  ArdJSON::JSONVar app = _savePending ? _pendingApp : _appConfig; app.remove(key);
  return scheduleConfig(_savePending ? _pending : _config, app, ChangeSource::Application, false);
}
bool ArdPortal::flushConfig() {
  if (!_configurationReady || !_savePending || _saveQueued) return false;
  _forceSave = true; return true;
}
void ArdPortal::notifyConfig() {
  ChangeSource source = _saveSource;
  if (_changed) _changed(_config, source);
  if (source == ChangeSource::Portal && _portalChanged) _portalChanged(_config);
}

void ArdPortal::finishSave(const ArdFS::Result& result) {
#if ARDPORTAL_ENABLE_MQTT
  const bool acknowledgeMqtt = _saveSource == ChangeSource::Mqtt;
#endif
  _savePending = _saveQueued = _forceSave = false;
  _storageOK = result.ok; _storageError = result.error;
  if (result.ok && result.changed) { _hasCommitted = true; _lastCommit = millis(); }
  bool respond = _httpWaitingStorage; _httpWaitingStorage = false;
#if ARDPORTAL_ENABLE_MQTT
  if (_mqttClient.trialSavePending()) {
    bool stillConnected = mqttConnected();
    if (result.ok) { _config = std::move(_pending); applyAppConfig(std::move(_pendingApp)); if (_saveHasChanges) notifyConfig(); }
    _mqttClient.completeTrialSave(result.ok, result.error, stillConnected);
  } else
#endif
  if (result.ok) {
    if (_resetAfterSave) {
      _config = std::move(_pending); applyAppConfig(std::move(_pendingApp)); notifyConfig();
      scheduleRestart(RestartReason::FactoryReset);
    } else if (!_wifiConnectAfterSave && ArdPortalJson::equal(_config, _pending)) {
      applyAppConfig(std::move(_pendingApp)); if (_saveHasChanges) notifyConfig();
    } else { _pendingReady = true; _notifyPending = _saveHasChanges; }
    if (respond) reply(_resetAfterSave ? 200 : 202, "text/plain", _resetAfterSave ?
      ArdUILanguage::text(ArdUILanguage::Key::s_174) : ArdUILanguage::text(ArdUILanguage::Key::s_175));
  } else {
    if (respond) reply(500, "text/plain", result.error);
    log(ArdUILanguage::text(ArdUILanguage::Key::s_176) + result.error);
  }
#if ARDPORTAL_ENABLE_MQTT
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  if (acknowledgeMqtt) { _appControls.acknowledgeSave(result.ok); }
#else
  (void)acknowledgeMqtt;
#endif
#endif
  _resetAfterSave = false; _wifiConnectAfterSave = false;
  if(!_savePending&&!_saveQueued&&!_pendingReady) {_pendingApp=ArdJSON::JSONVar::object();_pending.caCert=String();}
  if (_saved) _saved(result.ok, result.error);
}

// Application configuration remains available without dynamic form rendering.
void ArdPortal::applyAppConfig(ArdJSON::JSONVar app) {
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  _appControls.applyAppConfig(std::move(app));
#else
  ArdJSON::JSONVar previous=std::move(_appConfig);_appConfig=std::move(app);++_appRevision;
  if(!_appChanged)return;
  auto keys=previous.keys(),next=_appConfig.keys();
  for(size_t i=0;i<next.length();++i)if(!previous.hasOwnProperty(next[i].asString()))keys.push(next[i]);
  for(size_t i=0;i<keys.length();++i){String key=keys[i].asString();auto value=getAppConfigValue(key.c_str());if(_json.stringify(previous[key])!=_json.stringify(value))_appChanged(key,value,_saveSource);}
#endif
}
#if !ARDPORTAL_ENABLE_DYNAMIC_PAGES
bool ArdPortal::addPortalPage(const String&) {return false;}
bool ArdPortal::addPortalPage(const __FlashStringHelper*) {return false;}
bool ArdPortal::setAppStateValue(const char*,const ArdJSON::JSONVar&,bool) {return false;}
bool ArdPortal::queueAppStatePublish(const char*) {return false;}
bool ArdPortal::emitAppEvent(const char*,const ArdJSON::JSONVar&) {return false;}
#endif
