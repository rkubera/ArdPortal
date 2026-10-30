#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ArdFSHostVolume.h"
#include <cassert>
#include <type_traits>
#include <utility>
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0
#define ARDPORTAL_ENABLE_OTA 0
#define ARDPORTAL_ENABLE_MQTT 0
#define ARDPORTAL_ENABLE_HA 1
#define ARDPORTAL_ENABLE_DEPENDENCIES 1
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define private public
#include "ArdPortalDeclarations.h"
#undef private
#include "ArdPortal.h"
static_assert(!ARDPORTAL_ENABLE_HA && !ARDPORTAL_ENABLE_DEPENDENCIES && !ARDPORTAL_ENABLE_CONTROL_SLIDER, "Dynamic page dependencies must be forced off");
uint32_t fakeMillis = 0;
int ntpStarts = 0;
String lastNtpServer1, lastNtpServer2;
time_t fakeUnixTime = 1800000000;
SerialStub Serial;
WiFiStub WiFi;
EspStub ESP;
bool failMount = false, failFormat = false, failFileWrite = false;
std::map<std::string, std::vector<uint8_t>> fakeFiles;
std::deque<WiFiClient> WiFiServer::incoming;
bool WiFiClient::connectResult = true;
template<class T, class = void> struct hasRegistration : std::false_type {};
template<class T> struct hasRegistration<T, std::void_t<decltype(std::declval<T&>().startAppConfigPageRegistration(String()))>> : std::true_type {};
static_assert(!hasRegistration<ArdPortal>::value, "Disabled registration API must be absent");
int main() {
  ArdPortal portal;
#if ARDPORTAL_ENABLE_WWW
  portal._request = "GET / HTTP/1.1\r\nHost: device\r\n\r\n";
  portal.handleHttp();
  assert(portal._response.indexOf("Content-Encoding: gzip") >= 0);
  assert(portal._assetChunks && portal._pageLength > 2);
  assert(static_cast<unsigned char>(portal._page[0]) == 0x1f && static_cast<unsigned char>(portal._page[1]) == 0x8b);
  portal.closeHttp();
#endif
  int changes = 0;
  portal.onAppConfigValueChanged([&](const String& key, const ArdJSON::JSONVar& value, ArdPortal::ChangeSource) {
    assert(key == "a" && value.asDouble() == 42);
    ++changes;
  });
  auto app = ArdJSON::JSONVar::object();
  app["a"] = 42;
  portal.applyAppConfig(app);
  assert(portal.getAppConfigValue("a").asDouble() == 42 && changes == 1);
  portal.applyAppConfig(app);
  assert(changes == 1);
  portal.onAppConfigValueChanged({});
  // Generic application settings still persist through ArdFS without any form.
  assert(portal.begin());
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    portal.loop();
  }
  assert(portal.portalAndAppConfigReady());
  auto config = portal.getPortalConfig();
  config.password = u8"wifi-zażółć";
  config.apPassword = "ap-masked-pass";
  config.caCert = "-----BEGIN CERTIFICATE-----\nTEST\n-----END CERTIFICATE-----\n";
  config.deviceDescription = String("Description retained only in the active configuration after saving");
  assert(portal.setPortalConfig(config));
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    portal.loop();
  }
  assert(portal.setAppConfigValue("saved", 17));
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    portal.loop();
  }
  assert(portal.getAppConfigValue("saved").asDouble() == 17);
  assert(portal.getPortalConfig().deviceDescription == config.deviceDescription);
  assert(!portal._pending.deviceDescription.length());
  // The boot file stays plaintext, independently of the download transform.
  String storedFile;
  bool readDone = false;
  assert(portal._storage.readJsonConfig(
    "/ardportal.json", [&](size_t offset, const uint8_t* bytes, size_t length) {
      assert(offset == storedFile.length());
      return storedFile.concat(reinterpret_cast<const char*>(bytes), length);
    },
    [&](const ArdPortalStorage::Result& result) {
      assert(result.ok && result.found);
      readDone = true;
    }));
  for (int i = 0; i < 1000 && !readDone; ++i) portal.serviceStorage(fakeMillis);
  assert(readDone);
  auto bootDocument = ArdJSON::JSON.parse(storedFile);
#if ARDPORTAL_ENABLE_WIFI
  assert(bootDocument["config"]["password"].asString() == config.password);
  assert(bootDocument["config"]["apPassword"].asString() == config.apPassword);
#else
  assert(!bootDocument["config"].hasOwnProperty("ssid"));
  assert(!bootDocument["config"].hasOwnProperty("apPassword"));
#endif
  assert(!bootDocument["config"].hasOwnProperty("host"));
  assert(!bootDocument["config"].hasOwnProperty("port"));
  assert(!bootDocument["config"].hasOwnProperty("mqttTls"));
  assert(!bootDocument["config"].hasOwnProperty("caCert"));
#if ARDPORTAL_ENABLE_WWW && ARDPORTAL_ENABLE_BACKUP_RESTORE
  assert(portal.startConfigBackup());
  String backup;
  for (size_t offset = 0; offset < portal._httpBackupBytes;) {
    String part = portal.configDocumentSlice(portal._request, portal._httpBackupApp, false, portal._httpBackupBytes - portal._request.length() - 1, offset, 37);
    assert(part.length() && part.length() <= 37);
    backup += part;
    offset += part.length();
  }
  // Exercise the actual HTTP writer with socket backpressure.
  portal._http.endpoint = std::make_shared<Endpoint>();
  auto endpoint = portal._http.endpoint;
  endpoint->writeLimit = 7;
  for (int i = 0; i < 10000 && endpoint->connected; ++i) portal.serviceHttpIo(fakeMillis);
  String wire;
  for (auto byte : endpoint->output) wire += char(byte);
  assert(wire.substring(wire.indexOf("\r\n\r\n") + 4) == backup);
  auto snapshot = ArdJSON::JSON.parse(backup);
  assert(snapshot["config"]["deviceDescription"].asString() == config.deviceDescription);
  assert(snapshot["app"]["saved"].asDouble() == 17);
  assert(snapshot["config"]["password"].asString() != config.password);
  assert(snapshot["config"]["apPassword"].asString() != config.apPassword);
  assert(!snapshot["config"].hasOwnProperty("caCert"));
  portal.closeHttp();
  // Strict syntax rejection leaves both live settings and storage scheduling untouched.
  portal._request = "POST /api/restore HTTP/1.1\r\n\r\n{";
  assert(portal.startConfigRestore(1, portal._request.indexOf("\r\n\r\n")));
  assert(portal._response.indexOf("400") >= 0 && !portal._savePending);
  portal.closeHttp();
  portal._http.endpoint = std::make_shared<Endpoint>();
  portal._request = "POST /api/restore HTTP/1.1\r\n\r\n";
  assert(portal.startConfigRestore(backup.length(), portal._request.indexOf("\r\n\r\n")));
  for (size_t offset = 0; offset < backup.length(); ++offset) {
    portal._http.endpoint->input.push_back(uint8_t(backup[offset]));
    portal.serviceHttpIo(fakeMillis);
    assert(portal._request.length() == 0);
  }
  assert(portal._httpWaitingStorage && portal._savePending);
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    portal.serviceStorage(fakeMillis);
  }
  assert(portal._response.indexOf("202") >= 0);
  portal.closeHttp();
#endif
  ArdPortal restored;
  assert(restored.begin());
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    restored.loop();
  }
  assert(restored.portalAndAppConfigReady());
  assert(restored.getAppConfigValue("saved").asDouble() == 17);
#if ARDPORTAL_ENABLE_WIFI
  assert(restored.getPortalConfig().password == config.password);
  assert(restored.getPortalConfig().apPassword == config.apPassword);
#endif
  assert(!restored.getPortalConfig().caCert.length());
#if ARDPORTAL_USE_LITTLEFS && ARDPORTAL_ENABLE_WWW
  assert(restored.infoJson().indexOf("\"storageBackend\":\"LittleFS\"") >= 0);
  bool factoryCallback = false;
  restored.onBeforeFactoryReset([&]() {
    factoryCallback = true;
  });
  restored._request = "POST /api/factory-reset HTTP/1.1\r\n\r\nconfirm=RESET";
  restored.handleBuiltinHttp("POST", "/api/factory-reset");
  assert(factoryCallback);
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    restored.serviceStorage(fakeMillis);
  }
  assert(restored._rebootPending && !restored.portalAndAppConfigBusy());
  ArdPortal reset;
  assert(reset.begin());
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    reset.loop();
  }
  assert(reset.portalAndAppConfigReady() && !reset.getAppConfigValue("saved").isValid());
  reset._request = "POST /api/storage/format HTTP/1.1\r\n\r\nconfirm=FORMAT";
  reset.handleBuiltinHttp("POST", "/api/storage/format");
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    reset.serviceStorage(fakeMillis);
  }
  assert(reset._rebootPending && littlefs_test::files.empty());
#endif
}
