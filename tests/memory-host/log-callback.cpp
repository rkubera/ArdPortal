#include "Arduino.h"
#include "ESP8266WiFi.h"
#include "ArdFSHostVolume.h"
#include <cassert>
#define ARDPORTAL_ENABLE_LOG_CALLBACK 1
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0
#define ARDPORTAL_ENABLE_OTA 0
#define ARDPORTAL_ENABLE_HA 0
#define ARDPORTAL_ENABLE_MQTT_TLS 0
#define private public
#include "ArdPortalDeclarations.h"
#undef private
#include "ArdPortal.h"
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

static ArdPortal* owner;
static unsigned lines;
static void sink(ArdPortal::LogChannel channel, const String& text) {
  assert(channel == ArdPortal::LogChannel::Messages && text == "test");
  ++lines;
  owner->logMessage("nested");
}
int main() {
  ArdPortal portal;
  owner = &portal;
  portal.onLog(sink);
  portal.logMessage("test");
  assert(lines == 1);
  portal.onLog(nullptr);
  portal.logMessage("detached");
  assert(lines == 1);
#if ARDPORTAL_ENABLE_WWW
  static_assert(sizeof(portal._consoleWsRx) == (ARDPORTAL_ENABLE_MQTT ? 1032 : 136), "Correct RX size");
  auto endpoint = std::make_shared<Endpoint>();
  portal._consoleWs.client.endpoint = endpoint;
  // A maximum-size masked ping fits the small read-only buffer.
  endpoint->input = { 0x89, 0xfd, 0, 0, 0, 0 };
  for (unsigned i = 0; i < 125; ++i) endpoint->input.push_back('x');
  portal.serviceWebSocket(portal._consoleWs, 0);
  assert(endpoint->connected && !endpoint->output.empty() && (endpoint->output[0] & 15) == 10);
  endpoint->input = { 0x81, 0x81, 0, 0, 0, 0, 'x' };
  portal.serviceWebSocket(portal._consoleWs, 0);
  assert(endpoint->connected == bool(ARDPORTAL_ENABLE_MQTT));
#else
  static_assert(!ARDPORTAL_ENABLE_CONSOLE && !ARDPORTAL_SUPPORT_WEBSOCKET, "Headless excludes console");
#endif
}
