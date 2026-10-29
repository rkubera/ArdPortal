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
  ArdPortal restored;
  assert(restored.begin());
  for (int i = 0; i < 1000; ++i) {
    fakeMillis += 10;
    restored.loop();
  }
  assert(restored.portalAndAppConfigReady());
  assert(restored.getAppConfigValue("saved").asDouble() == 17);
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
