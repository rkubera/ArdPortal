#include "ZigbeeFields.h"
#define private public
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 1
#define ARDPORTAL_ZIGBEE_ROUTER 0
#define ARDPORTAL_ZIGBEE_OTA_SUPPORT 1
#define ARDPORTAL_ZIGBEE_OTA_MANUFACTURER 0xffff
#define ARDPORTAL_ZIGBEE_OTA_IMAGE_TYPE 1
#define ARDPORTAL_ZIGBEE_OTA_FILE_VERSION 1
#include "ArdZigbeeImpl.h"
#undef private
#include <cassert>
uint32_t fakeMillis = 0;
time_t fakeUnixTime = 0;
EspStub ESP;
/** @brief Test actual native OTA callbacks with fragmented subelements and failing flash operations. @return Zero on success. */
int main() {
  ArdZigbee z;
  ArdZigbeeFields::Spec spec;
  spec.kind = ArdZigbeeFields::Kind::Update;
  assert(z.addControl(spec, ArdZigbeeFields::Values(), "Firmware"));
  assert(!z.addControl(spec, ArdZigbeeFields::Values(), "Duplicate"));
  assert(z.createControls(&nativeEndpoints));
  assert(z.begin(ArdZigbee::Options()));
  esp_zb_zcl_ota_upgrade_value_message_t m{};
  m.upgrade_status = ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START;
  assert(z.otaAction(&m) != ESP_OK && !z.otaActive());
  z.allowOta(true);
  assert(z.otaAction(&m) == ESP_OK && z.otaActive());
  uint8_t bytes[] = { 0, 0, 4, 0, 0, 0, 0xe9, 1, 2, 3 };
  m.upgrade_status = ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE;
  for (auto& b : bytes) {
    m.payload = &b;
    m.payload_size = 1;
    assert(z.otaAction(&m) == ESP_OK);
  }
  assert(otaBytes.size() == 4 && otaBytes[0] == 0xe9);
  m.upgrade_status = ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK;
  assert(z.otaAction(&m) == ESP_OK);
  m.upgrade_status = ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH;
  assert(z.otaAction(&m) == ESP_OK && otaBoots == 1 && z.takeOtaFinished() && !z.takeOtaFinished());
  for (int failure = 0; failure < 5; ++failure) {
    z.allowOta(true);
    m.upgrade_status = ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START;
    otaBeginFail = failure == 0;
    int result = z.otaAction(&m);
    otaBeginFail = false;
    if (failure == 0) {
      assert(result != ESP_OK);
      continue;
    }
    m.upgrade_status = ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE;
    m.payload = bytes;
    m.payload_size = sizeof(bytes);
    otaWriteFail = failure == 1;
    result = z.otaAction(&m);
    otaWriteFail = false;
    if (failure == 1) {
      assert(result != ESP_OK && !z.otaActive());
      continue;
    }
    otaEndFail = failure == 2;
    otaBootFail = failure == 3;
    m.upgrade_status = failure == 4 ? ESP_ZB_ZCL_OTA_UPGRADE_STATUS_ABORT : ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH;
    assert(z.otaAction(&m) != ESP_OK && !z.otaActive() && !z.takeOtaFinished());
    otaEndFail = otaBootFail = false;
  }
  assert(otaBoots == 1);
  z.allowOta(true);
  m.upgrade_status = ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START;
  assert(z.otaAction(&m) == ESP_OK);  // A callback can refresh activity after loop() sampled its `now` argument.
  z._otaLastActivity = fakeMillis + 1;
  z.loop(fakeMillis);
  assert(z.otaActive());
  // The same ordering must remain safe at the millis() rollover.
  fakeMillis = 0xffffffffU;
  z._otaLastActivity = 0;
  z.loop(fakeMillis);
  assert(z.otaActive());
  fakeMillis = 59999;
  z.loop(fakeMillis);
  assert(z.otaActive());
  fakeMillis = 60000;
  z.loop(fakeMillis);
  assert(!z.otaActive());
  ArdZigbeeOtaImage parser;
  uint8_t invalid[] = { 1, 0, 1, 0, 0, 0, 0xe9 };
  assert(!parser.feed(invalid, sizeof(invalid), 4096, [](const uint8_t*, size_t) {
    return true;
  }));
  parser.reset();
  assert(!parser.feed(bytes, sizeof(bytes), 3, [](const uint8_t*, size_t) {
    return true;
  }));
  parser.reset();
  assert(parser.feed(bytes, sizeof(bytes) - 1, 4096, [](const uint8_t*, size_t) {
    return true;
  }) && !parser.complete());
  puts("PASS: native OTA admission, split headers, exact image length, flash failures, abort, timeout and deferred verified restart");
}
