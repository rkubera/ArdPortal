#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ArdFSHostVolume.h"
#include <cassert>
#include "Updater.h"
#include "WiFiClientSecure.h"
#define ARDPORTAL_TEST_ALLOCATE testAllocate
#define ARDPORTAL_DEFINE_HA_DATA
#define private public
#include "../../src/ArdPortalDeclarations.h"
#undef private
#undef ARDPORTAL_DEFINE_HA_DATA
#include "ArdPortal.h"
#include "../../src/WebSocketAccept.h"
#include "../../src/ConfigJson.h"
#include "../../src/Language.h"
long allocationBudget = -1;
size_t allocations = 0;
void* testAllocate(size_t bytes) noexcept {
  if (allocationBudget == 0) return nullptr;
  if (allocationBudget > 0) --allocationBudget;
  ++allocations;
  return std::malloc(bytes);
}
uint32_t fakeMillis = 0;
int ntpStarts = 0;
String lastNtpServer1, lastNtpServer2;
time_t fakeUnixTime = 1800000000;
int WiFiClientSecure::tlsAttempts = 0, WiFiClientSecure::tlsWrites = 0;
String WiFiClientSecure::lastHost;
bool WiFiClientSecure::tlsConnectResult = true;
SerialStub Serial;
WiFiStub WiFi;
EspStub ESP;
UpdateStub Update;
bool failMount = false, failFormat = false;
bool WiFiClient::connectResult = true;
std::deque<WiFiClient> WiFiServer::incoming;
std::map<std::string, std::vector<uint8_t>> fakeFiles;
bool failFileWrite = false, failRename = false;


void drain(ArdPortal& p) {
  if (p._savePending && !p._saveQueued) p.flushPortalAndAppConfig();
  for (int i = 0; i < 10000 && (p.portalAndAppConfigBusy() || !p.portalAndAppConfigReady()); ++i) {
    fakeMillis += 10;
    p.serviceStorage(fakeMillis);
  }
  assert(!p._storage.busy() && !p._savePending);
}
int main() {
  {
    ArdPortal p;
    auto& mqtt = p._mqttClient;
    const auto savedMillis = fakeMillis;
    const int savedWifi = WiFi.state;
    WiFi.state = WL_CONNECTED;
    p._config.host = "broker";
    mqtt._mqttState = ArdPortal::MqttState::Connected;
    mqtt._lastTx = fakeMillis;
    assert(mqtt._mqtt.connect(IPAddress(), 1883));
    auto endpoint = mqtt._mqtt.endpoint;
    endpoint->writeLimit = 7;
    String body;
    for (size_t i = 0; i < 130; ++i) body += char(i);
    assert(mqtt.queuePacket(0x30, body));
    assert(mqtt._tx.length() == 133);
    mqtt.serviceMqtt(++fakeMillis);
    assert(mqtt._txSize == 133 && mqtt._txOffset == 7 && mqtt._tx.length() == 133);
    for (size_t i = 0; i < 30 && mqtt._txSize; ++i) mqtt.serviceMqtt(++fakeMillis);
    assert(!mqtt._txSize && !mqtt._tx.length() && endpoint->output.size() == 133);
    assert(endpoint->output[0] == 0x30 && endpoint->output[1] == 0x82 && endpoint->output[2] == 1);
    assert(!memcmp(endpoint->output.data() + 3, body.c_str(), body.length()));
    assert(mqtt.queuePacket(0x30, body));
    mqtt.closeMqtt();
    assert(!mqtt._txSize && !mqtt._tx.length());
    WiFi.state = savedWifi;
    fakeMillis = savedMillis;
  }
  {
    ArdPortal p;
    assert(String(p.getFirmwareVersion()) == "1.0");
    assert(ArdJSON::JSON.parse(p.infoJson())["firmwareVersion"].asString() == "1.0");
    auto f = ArdJSON::JSON.parse(R"({"id":"version_test","type":"switch","name":"Relay"})");
    auto d = ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(f));
    assert(d["device"]["sw_version"].asString() == "1.0");
    assert(!p.setFirmwareVersion(nullptr) && !p.setFirmwareVersion("") && !p.setFirmwareVersion("12345678901234567"));
    assert(p.setFirmwareVersion("2.3.4-beta"));
    d = ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(f));
    assert(d["device"]["sw_version"].asString() == "2.3.4-beta");
    assert(ArdJSON::JSON.parse(p.infoJson())["firmwareVersion"].asString() == "2.3.4-beta");
    p._started = true;
    assert(!p.setFirmwareVersion("3.0"));
    assert(String(p.getFirmwareVersion()) == "2.3.4-beta");
  }

  {
    ArdPortal p;
    auto f = ArdJSON::JSON.parse(R"({"id":"room","type":"climate","name":"Room","min":10,"max":35,"step":0.5,"modes":["off","heat","cool","heat_cool"]})");
    auto value = ArdDynamicPages::initial(f);
    assert(value["temperature_low"].asDouble() == 10 && value["temperature_high"].asDouble() == 35);
    value["mode"] = "heat_cool";
    value["temperature_low"] = 20;
    value["temperature_high"] = 25;
    assert(ArdDynamicPages::validValue(f, value));
    auto discovery = ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(f));
    assert(discovery["temperature_low_command_topic"].asString().indexOf("room_temperature_low") >= 0);
    assert(discovery["temperature_high_state_template"].asString() == "{{ value_json.temperature_high }}");
    f["mqtt"]["state_topic"] = "custom/climate";
    discovery = ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(f));
    assert(discovery["temperature_low_state_topic"].asString() == "custom/climate");
    assert(discovery["mode_state_topic"].asString() == "custom/climate");
    value["temperature_low"] = 26;
    assert(!ArdDynamicPages::validValue(f, value));
    value["temperature_low"] = 20.25;
    assert(!ArdDynamicPages::validValue(f, value));
    value.remove("temperature_low");
    assert(!ArdDynamicPages::validValue(f, value));
    value["mode"] = "cool";
    assert(ArdDynamicPages::validValue(f, value));
  }

  {
    ArdPortal::PortalConfig bandConfig;
    bandConfig.deviceName = "Band test";
    bandConfig.apBand = 5;
    String encoded = ArdPortalJson::encode(bandConfig);
    ArdPortal::PortalConfig restored;
    assert(encoded.length() && ArdPortalJson::decode(encoded, restored) && restored.apBand == 5);
    auto legacy = ArdPortalJson::value(bandConfig);
    legacy.remove("apBand");
    assert(ArdPortalJson::decodeValue(legacy, restored) && restored.apBand == 24);
    auto invalid = ArdPortalJson::value(bandConfig);
    invalid["apBand"] = 6;
    assert(!ArdPortalJson::decodeValue(invalid, restored));
  }

  {
    ArdPortal discovery;
    auto field = ArdJSON::JSON.parse(R"({"id":"toggle","name":"Toggle","type":"switch","default":false})");
    auto config = ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config.isValid() && !config.hasOwnProperty("optimistic"));
    field["mqtt"]["state_topic"] = "custom/mqtt/toggle";
    field["ha"]["state_topic"] = "legacy/toggle";
    config = ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config["state_topic"].asString() == "custom/mqtt/toggle");
    field["mqtt"]["state_topic"] = "";
    config = ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config["state_topic"].asString() == discovery._mqttClient.mqttTopic("stat", "toggle"));
    field.remove("mqtt");
    config = ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config["state_topic"].asString() == "legacy/toggle");
    field["ha"]["optimistic"] = true;
    config = ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config["optimistic"].asBool());
    assert(config["state_topic"].asString().length() > 0 && config["command_topic"].asString().length() > 0);
    assert(config["icon"].asString() == "mdi:toggle-switch");
    field["icon"] = "mdi:fan";
    config = ArdJSON::JSON.parse(discovery._homeAssistant.discoveryConfig(field));
    assert(config["icon"].asString() == "mdi:fan");
  }
  {
    ArdPortal p;
    auto plain = ArdJSON::JSON.parse(R"({"id":"reading","name":"Reading","type":"text"})");
    assert(ArdDynamicPages::initial(plain).asString() == "");
    auto config = ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(plain));
    assert(!config.hasOwnProperty("device_class") && !config.hasOwnProperty("state_class"));
    for (auto type : { "number", "temperature", "humidity", "pressure", "illuminance", "flow", "carbon_dioxide", "pm25", "wind_speed", "voltage", "current", "power" }) {
      auto field = plain;
      field["textType"] = type;
      assert(ArdDynamicPages::initial(field).isNull());
      assert(ArdDynamicPages::validValue(field, nullptr) && ArdDynamicPages::validValue(field, 22.5) && !ArdDynamicPages::validValue(field, "22.5"));
      config = ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(field));
      auto pattern = ArdText::pattern(field);
      assert(config["state_class"].asString() == "measurement");
      assert(config["device_class"].asString() == pattern.device && config["unit_of_measurement"].asString() == pattern.unit);
      field["ha"]["value_template"] = "{{ value | float * 2 }}";
      config = ArdJSON::JSON.parse(p._homeAssistant.discoveryConfig(field));
      assert(config["value_template"].asString() == "{{ value | float * 2 }}");
    }
    plain["textType"] = "bogus";
    assert(!p._dynamic.validateField(plain, ArdJSON::JSONVar::array(), false));
    assert(String(p._dynamic.error.stage) == "field" && p._dynamic.error.fieldId == "reading" && p._dynamic.error.reason == "unsupported textType");
  }
  // Rapid edits across multiple keys must preserve the last accepted intent, including across reboot.
  {
    ArdPortal rapid;
    assert(rapid.begin());
    drain(rapid);
    int saves = 0;
    rapid.onPortalAndAppConfigSaved([&](bool ok, const String&) {
      assert(ok);
      ++saves;
    });
    ArdJSON::JSONVar expected = ArdJSON::JSONVar::object();
    for (int i = 0; i < 120; ++i) {
      const char* key = i % 3 == 0 ? "stressRelay" : i % 3 == 1 ? "stressSetpoint"
                                                                : "stressProfile";
      ArdJSON::JSONVar value = i % 3 == 0 ? ArdJSON::JSONVar(bool(i % 2)) : i % 3 == 1 ? ArdJSON::JSONVar(i)
                                                                                       : ArdJSON::JSONVar(i % 2 ? "eco" : "normal");
      assert(rapid.setAppConfigValue(key, value));
      expected[key] = value;
      if (i % 12 == 11) rapid.flushPortalAndAppConfig();
      fakeMillis += 20;
      rapid.serviceStorage(fakeMillis);
    }
    assert(expected.length() == 3);
    drain(rapid);
    assert(saves >= 2);
    // Deletions and repeated edits during the active snapshot belong to the next save.
    assert(rapid.setAppConfigValue("removeTarget", 1));
    assert(rapid.flushPortalAndAppConfig());
    fakeMillis += 10;
    rapid.serviceStorage(fakeMillis);
    assert(rapid._saveQueued);
    assert(rapid.removeAppConfigValue("removeTarget"));
    assert(rapid.setAppConfigValue("queuedValue", 41));
    assert(rapid.setAppConfigValue("queuedValue", 42));
    assert(rapid.portalAndAppConfigBusy());
    assert(rapid.flushPortalAndAppConfig());
    // A rejected allocation cannot destroy a previously accepted queued value.
    allocationBudget = 0;
    assert(!rapid.setAppConfigValue("queuedValue", 43));
    allocationBudget = -1;
    ArdJSON::JSONVar rejected = ArdJSON::JSONVar::object();
    rejected["queuedValue"] = 43;
    allocationBudget = 0;
    assert(!rapid.scheduleAppPatch(std::move(rejected), ArdPortal::ChangeSource::Application));
    allocationBudget = -1;
    drain(rapid);
    assert(!rapid._appConfig.hasOwnProperty("removeTarget"));
    assert(rapid.getAppConfigValue("queuedValue").asDouble() == 42);
    // Application edits also queue behind a pending save from a different source.
    ArdJSON::JSONVar incoming = ArdJSON::JSONVar::object();
    incoming["crossSource"] = 7;
    assert(rapid.scheduleAppPatch(std::move(incoming), ArdPortal::ChangeSource::Mqtt));
    assert(rapid.setAppConfigValue("crossSource", 8));
    drain(rapid);
    assert(rapid.getAppConfigValue("crossSource").asDouble() == 8);
    ArdPortal rebooted;
    assert(rebooted.begin());
    drain(rebooted);
    assert(!rebooted._appConfig.hasOwnProperty("removeTarget") && rebooted.getAppConfigValue("queuedValue").asDouble() == 42);
    expected.forEachObjectMember([&](const String& key, const ArdJSON::JSONVar& value) {
      assert(ArdJSON::JSON.stringify(rebooted.getAppConfigValue(key.c_str())) == ArdJSON::JSON.stringify(value));
      return true;
    });
  }
  fakeFiles.clear();
  // Failed snapshots survive two retries, while later edits wait in their own batch.
  {
    ArdPortal retry;
    assert(retry.begin());
    drain(retry);
    assert(retry.setAppConfigValue("keep", 11));
    assert(retry.setAppConfigValue("remove", 7));
    drain(retry);
    std::vector<bool> outcomes;
    std::vector<uint8_t> attempts;
    std::vector<uint32_t> failures;
    int terminal = 0;
    retry.onAppConfigValueSaveFailed([&](const String&, const ArdJSON::JSONVar&, uint8_t, const String&) {
      ++terminal;
    });
    retry.onPortalAndAppConfigSaved([&](bool ok, const String& error) {
      outcomes.push_back(ok);
      attempts.push_back(retry.portalAndAppConfigSaveAttempts());
      if (!ok) {
        assert(error.length() && retry._appControls._mqttAckInFlight);
        failures.push_back(fakeMillis);
      }
    });
    ArdJSON::JSONVar mqttPatch = ArdJSON::JSONVar::object();
    mqttPatch["keep"] = 22;
    mqttPatch["remove"] = ArdJSON::JSONVar();
    assert(retry.scheduleAppPatch(std::move(mqttPatch), ArdPortal::ChangeSource::Mqtt));
    retry._appControls._mqttAckInFlight = ArdAppConfigFieldMask::forField(0);
    assert(retry.flushPortalAndAppConfig());
    failFileWrite = true;
    fakeMillis += 10;
    retry.serviceStorage(fakeMillis);
    assert(retry._saveQueued);
    assert(retry.setAppConfigValue("keep", 33));
    assert(retry.setAppConfigValue("extra", 99));
    for (int i = 0; i < 10000 && failures.size() < 2; ++i) {
      fakeMillis += 10;
      retry.flushPortalAndAppConfig();
      retry.serviceStorage(fakeMillis - 5);
    }
    assert(failures.size() == 2 && failures[1] - failures[0] >= 1000 && retry.portalAndAppConfigBusy());
    failFileWrite = false;
    drain(retry);
    assert(outcomes.size() == 4 && !outcomes[0] && !outcomes[1] && outcomes[2] && outcomes[3]);
    assert(attempts[0] == 1 && attempts[1] == 2 && attempts[2] == 3 && attempts[3] == 1 && terminal == 0);
    assert(!retry._appControls._mqttAckInFlight);
    ArdPortal reboot;
    assert(reboot.begin());
    drain(reboot);
    assert(reboot.getAppConfigValue("keep").asDouble() == 33 && reboot.getAppConfigValue("extra").asDouble() == 99);
    assert(!reboot._appConfig.hasOwnProperty("remove"));
  }
  fakeFiles.clear();
  // Three failures exhaust a full snapshot, report deletions, and leave later batches independent.
  {
    ArdPortal retry;
    assert(retry.begin());
    drain(retry);
    assert(retry.setAppConfigValue("remove", 7));
    drain(retry);
    int failures = 0, terminal = 0;
    bool failedA = false, failedDeletion = false;
    retry.onPortalAndAppConfigSaved([&](bool ok, const String&) {
      if (!ok) {
        ++failures;
        assert(retry.portalAndAppConfigSaveAttempts() == failures);
      }
    });
    retry.onAppConfigValueSaveFailed([&](const String& key, const ArdJSON::JSONVar& value, uint8_t attempts, const String& error) {
      assert(attempts == 3 && error.length());
      ++terminal;
      if (key == "failed") {
        assert(value.asDouble() == 42);
        failedA = true;
      }
      if (key == "remove") {
        assert(value.isUndefined());
        failedDeletion = true;
      }
    });
    ArdJSON::JSONVar full = ArdJSON::JSONVar::object();
    full["failed"] = 42;
    assert(retry.scheduleConfig(retry.getPortalConfig(), full, ArdPortal::ChangeSource::Application, true));
    failFileWrite = true;
    drain(retry);
    assert(failures == 3 && terminal == 2 && failedA && failedDeletion);
    assert(!retry.portalAndAppConfigBusy() && retry.portalAndAppConfigSaveAttempts() == 3);
    const auto writes = fakeOpenWrites;
    failFileWrite = false;
    for (int i = 0; i < 2000; ++i) {
      fakeMillis += 10;
      retry.serviceStorage(fakeMillis);
    }
    assert(fakeOpenWrites == writes && failures == 3 && terminal == 2);
    assert(retry.setAppConfigValue("recovery", 1));
    drain(retry);
    assert(retry.portalAndAppConfigSaveAttempts() == 1);
    ArdPortal reboot;
    assert(reboot.begin());
    drain(reboot);
    assert(!reboot._appConfig.hasOwnProperty("failed") && reboot.getAppConfigValue("remove").asDouble() == 7);
    assert(reboot.getAppConfigValue("recovery").asDouble() == 1);
    // Exhausting the older batch does not discard a newer value already accepted for the same key.
    failures = 0;
    int exhaustedAgain = 0;
    retry.onAppConfigValueSaveFailed([&](const String& key, const ArdJSON::JSONVar& value, uint8_t count, const String&) {
      assert(key == "latest" && value.asDouble() == 1 && count == 3);
      ++exhaustedAgain;
      failFileWrite = false;
      assert(retry.setAppConfigValue("fromCallback", true));
    });
    assert(retry.setAppConfigValue("latest", 1));
    assert(retry.flushPortalAndAppConfig());
    failFileWrite = true;
    fakeMillis += 10;
    retry.serviceStorage(fakeMillis);
    assert(retry._saveQueued);
    assert(retry.setAppConfigValue("latest", 2));
    drain(retry);
    assert(failures == 3 && exhaustedAgain == 1 && retry.getAppConfigValue("latest").asDouble() == 2);
    assert(retry.getAppConfigValue("fromCallback").asBool());
  }
  fakeFiles.clear();
  // Startup repairs a missing alternate before readiness; failed repair restores data but blocks edits.
  {
    {
      ArdPortal first;
      assert(first.begin());
      drain(first);
      assert(first.setAppConfigValue("bootRepair", 42));
      drain(first);
    }
    const auto source = fakeFiles["/ardportal.json"];
    failFileWrite = true;
    {
      ArdPortal blocked;
      bool ready = false;
      blocked.onPortalAndAppConfigReady([&](bool restored) {
        assert(restored);
        ready = true;
      });
      assert(blocked.begin());
      drain(blocked);
      assert(ready && !blocked.portalAndAppConfigStorageOK());
      assert(blocked.getAppConfigValue("bootRepair").asDouble() == 42 && !blocked.setAppConfigValue("bootRepair", 43));
    }
    failFileWrite = false;
    assert(fakeFiles["/ardportal.json"] == source);
    {
      ArdPortal fixed;
      assert(fixed.begin());
      drain(fixed);
      assert(fixed.portalAndAppConfigStorageOK());
      assert(fixed.getAppConfigValue("bootRepair").asDouble() == 42 && fixed.setAppConfigValue("bootRepair", 43));
      drain(fixed);
    }
  }
  fakeFiles.clear();
  using V = ArdJSON::JSONVar;
  ArdPortal p;
  assert(p.begin());
  drain(p);
  for (int i = 0; i < 300; ++i) p._appConfig[String("setting") + String(i)] = i;
  allocationBudget = 12;
  size_t before = allocations;
  assert(p.setAppConfigValue("setting1", 17));
  assert(allocations - before < 12);
  allocationBudget = -1;
  assert(p._pendingAppPatch && p._pendingApp.length() == 1);
  assert(p.getAppConfigValue("setting1").asDouble() == 1);
  assert(p.setAppConfigValue("setting2", 18));
  assert(p._pendingApp.length() == 2);
  drain(p);
  assert(p._appConfig.length() == 300 && p.getAppConfigValue("setting1").asDouble() == 17);
  assert(p.removeAppConfigValue("setting3"));
  assert(p._pendingApp.length() == 1);
  assert(p.setAppConfigValue("setting4", 44));
  drain(p);
  assert(!p._appConfig.hasOwnProperty("setting3") && p._appConfig.length() == 299);
  // Changing portal settings while field edits are pending must preserve all values.
  assert(p.setAppConfigValue("setting5", 55));
  auto config = p.getPortalConfig();
  config.deviceDescription = "changed";
  assert(p.setPortalConfig(config));
  drain(p);
  if (p._pendingReady) {
    p._config = std::move(p._pending);
    p.applyPendingApp();
    p._pendingReady = false;
  }
  assert(p._appConfig.length() == 299 && p.getAppConfigValue("setting5").asDouble() == 55);
  assert(p.startAppConfigPageRegistration(String(R"({"id":"test","name":"Test","order":0,"fields":[{"id":"setting1","name":"Slider","type":"slider","min":0,"max":100} ]})")));
  for (int i = 0; i < 1000 && p.appConfigPageRegistrationBusy(); ++i) p.serviceAppConfigPageRegistration();
  if (p._dynamic.count() != 1) {
    auto e = p.appConfigRegistrationError();
    fprintf(stderr, "reg %s %s %s busy=%d\n", e.stage, e.fieldId.c_str(), e.reason.c_str(), int(p.appConfigPageRegistrationBusy()));
  }
  assert(p._dynamic.count() == 1);
  int callbacks = 0;
  p.onAppConfigValueChanged([&](const String&, const V&, ArdPortal::ChangeSource) {
    ++callbacks;
  });
  p._request = "POST /api/app HTTP/1.1\r\nContent-Length: 41\r\n\r\n{\"page\":\"test\",\"values\":{\"setting1\":22}}";
  p.handleHttp();
  assert(p._response.indexOf("202") >= 0);
  assert(p._pendingAppPatch && p._pendingApp.length() == 1);
  assert(p.getAppConfigValue("setting1").asDouble() == 22);
  assert(callbacks == 1);
  p.closeHttp();
  drain(p);
  assert(callbacks == 1 && p._appConfig["setting1"].asDouble() == 22);
  // MQTT persists only its affected fields and acknowledges after verification.
  assert(p._appControls.acceptMqttState("setting1", V(23)));
  p._appControls.prepareMqtt(fakeMillis);
  assert(p._pendingAppPatch && p._pendingApp.length() == 1);
  drain(p);
  assert(p._appConfig["setting1"].asDouble() == 23);
  ArdPortal restored;
  assert(restored.begin());
  drain(restored);
  assert(restored._appConfig.length() == 299 && restored.getAppConfigValue("setting1").asDouble() == 23 && !restored._appConfig.hasOwnProperty("setting3"));
}
