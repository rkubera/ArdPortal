// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT

/*
 * DHT
 *
 * Purpose: Expose real DHT temperature and humidity readings in the portal and MQTT/HA.
 *
 * Demonstrates: Sensor configuration, calibration offsets, numeric measurements and live
 * updates.
 *
 * Requirements: ESP8266/ESP32, Adafruit DHT and Unified Sensor libraries; wiring notes below.
 *
 * What to expect: Home shows Celsius/% readings; Settings selects GPIO/type, offsets and
 * reporting rate.
 */

/*
 * Configurable DHT (GPIO2 / DHT22 by default) with a small Wi-Fi portal, MQTT, Home Assistant, Console and OTA.
 * Home shows readings; Settings select GPIO, sensor type, two offsets and MQTT update rate.
 * Install Adafruit's DHT sensor library and its Unified Sensor dependency.
 * Connect VCC to 3.3 V, GND to GND and DATA to the selected GPIO with a 4.7-10 kOhm
 * pull-up to 3.3 V. GPIO2 must remain high during ESP8266 boot (not D2/GPIO4).
 * Use GPIO numbers, not board labels such as D2; avoid pins reserved by your board.
 * ESP8266 serial flashing also drives UART TX on GPIO2. If DHT stops responding
 * after flashing, power-cycle both devices; restarting ESP alone does not reset DHT.
 * MQTT TLS, dependent fields and unused controls are compiled out.
 * Readings update RAM only; sensor settings and calibration are saved by ArdFS.
 * DHT reads are synchronous (~25 ms normally), scheduled every two seconds.
 * Readings are numbers in Celsius and percent, or null when unavailable. HA handles display unit conversion.
 */
#define ARDPORTAL_ENABLE_OTA 1
#define ARDPORTAL_ENABLE_MQTT 1
#define ARDPORTAL_ENABLE_MQTT_TLS 0
#define ARDPORTAL_ENABLE_HA 1
#define ARDPORTAL_ENABLE_CONSOLE 1
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1
#define ARDPORTAL_ENABLE_DEPENDENCIES 0
#define ARDPORTAL_ENABLE_CONTROLS 0
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 1
#define ARDPORTAL_ENABLE_CONTROL_SELECT 1
#include <ArdPortal.h>
#include <DHT.h>
#if defined(ESP32)
#include <driver/gpio.h>
#endif

constexpr uint8_t DHT_PIN = 2;
constexpr uint32_t READ_INTERVAL_MS = 2000;
ArdPortal portal;
DHT dht(DHT_PIN, DHT22);
uint8_t activeDhtPin = DHT_PIN;
bool sensorSettingsChanged = false;
bool firstReadingPending = true;
float rawTemperature = NAN, rawHumidity = NAN;
uint32_t lastRead = 0, lastMqttUpdate = 0, mqttIntervalMs = 0;
bool readingsChanged = false, measurementSettingsChanged = false;
uint8_t failedReads = 0;

static const char HOME_PAGE[] PROGMEM = R"JSON({
  "id":"home","name":"Home","order":0,"fields":[
    {"id":"temperature","type":"text","class":"temperature","name":"Temperature","default":null,"persist":false},
    {"id":"humidity","type":"text","class":"humidity","name":"Humidity","default":null,"persist":false}
  ]
})JSON";

static const char SENSOR_PAGE[] PROGMEM = R"JSON({
  "id":"settings","name":"Settings","order":10,"fields":[
    {"id":"dht_pin","type":"select","name":"DHT data pin (GPIO)","default":"2","options":[DHT_PIN_OPTIONS]},
    {"id":"dht_type","type":"select","name":"DHT sensor type","default":"22",
     "options":[{"value":"11","name":"DHT11"},{"value":"12","name":"DHT12"},
                {"value":"21","name":"DHT21 / AM2301"},{"value":"22","name":"DHT22 / AM2302"}]},
    {"id":"temperature_offset","type":"slider","name":"Temperature offset (°C)","min":-10,"max":10,"step":0.1,"default":0},
    {"id":"humidity_offset","type":"slider","name":"Humidity offset (%)","min":-20,"max":20,"step":1,"default":0},
    {"id":"mqtt_update","type":"select","name":"MQTT updates","default":"0",
     "options":[{"value":"0","name":"On reading change"},{"value":"10","name":"Every 10 seconds"},
                {"value":"60","name":"Every minute"},{"value":"600","name":"Every 10 minutes"},
                {"value":"3600","name":"Every hour"}]}
  ]
})JSON";

/**
 * @brief Keep diagnostics in the portal's Console > Messages and on Serial.
 * @param message Complete diagnostic line.
 * @return No value.
 */
void logDiagnostic(const String& message) {
  portal.log(message);
  Serial.println(message);
}

/**
 * @brief Register settings with GPIO choices supported by the target chip.
 * @return Whether registration started successfully.
 */
bool registerSensorPage() {
  String pins;
#if defined(ESP32)
  for (unsigned pin = 0; pin < GPIO_NUM_MAX; ++pin) {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) continue;
#else
  for (unsigned pin = 0; pin <= 16; ++pin) {
    if (pin >= 6 && pin <= 11) continue;  // ESP8266 flash pins.
#endif
    if (pins.length()) pins += ',';
    pins += F("{\"value\":\"");
    pins += pin;
    pins += F("\",\"name\":\"GPIO");
    pins += pin;
    pins += F("\"}");
  }
  String page = FPSTR(SENSOR_PAGE);
  page.replace("DHT_PIN_OPTIONS", pins);
  return portal.startAppConfigPageRegistration(page);
}

/**
 * @brief Apply saved hardware settings and discard samples from the old sensor.
 * @return No value.
 */
void configureSensor() {
  pinMode(activeDhtPin, INPUT);  // Release the previous data pin, including its pull-up.
  activeDhtPin = uint8_t(atoi(portal.getAppConfigValue("dht_pin").asString().c_str()));
  uint8_t type = uint8_t(atoi(portal.getAppConfigValue("dht_type").asString().c_str()));
  dht = DHT(activeDhtPin, type);
  dht.begin();
  rawTemperature = rawHumidity = NAN;
  failedReads = 0;
  firstReadingPending = true;
  logDiagnostic(String(F("DHT: configured GPIO")) + String(activeDhtPin) + F(", type DHT") + String(type));
  lastRead = millis();  // Allow two seconds before reading the newly selected sensor.
  sensorSettingsChanged = false;
  updateReadings();
  measurementSettingsChanged = true;
}

/**
 * @brief Update measurement.
 * @param key Configuration key or JSON object member name.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return No value.
 */
void updateMeasurement(const char* key, const ArdJSON::JSONVar& value) {
  readingsChanged |= ArdJSON::JSON.stringify(portal.getAppConfigValue(key)) != ArdJSON::JSON.stringify(value);
  // Live portal updates are separate from the MQTT publishing schedule.
  if (!portal.setAppConfigStateValue(key, value, false)) logDiagnostic(F("DHT: portal value rejected."));
}

/**
 * @brief Load mqtt interval.
 * @return No value.
 */
void loadMqttInterval() {
  mqttIntervalMs = uint32_t(atoi(portal.getAppConfigValue("mqtt_update").asString().c_str())) * 1000;
  lastMqttUpdate = millis();
}

/**
 * @brief Advance mqtt updates.
 * @param now Current time used to evaluate deadlines.
 * @return No value.
 */
void serviceMqttUpdates(uint32_t now) {
  if (!portal.mqttConnected() || portal.ota().active()) return;
  if (!measurementSettingsChanged && (mqttIntervalMs ? uint32_t(now - lastMqttUpdate) < mqttIntervalMs : !readingsChanged)) return;
  // Only queue state: the portal publishes one field at a time and yields.
  if (portal.queueAppConfigStatePublish("temperature") && portal.queueAppConfigStatePublish("humidity")) {
    lastMqttUpdate = now;
    readingsChanged = false;
    measurementSettingsChanged = false;
  }
}

/**
 * @brief Round a bounded DHT reading to tenths.
 * @param number Sensor reading to round.
 * @return Signed count of tenths.
 */
int roundedTenths(float number) {
  return int(number * 10 + (number < 0 ? -0.5f : 0.5f));
}

/**
 * @brief Format a reading for diagnostics without the general float-to-string formatter.
 * @param number Sensor reading to round.
 * @return The reading rounded to one decimal place.
 */
String oneDecimal(float number) {
  int tenths = roundedTenths(number);
  String value;
  if (tenths < 0) {
    value += '-';
    tenths = -tenths;
  }
  value += String(tenths / 10);
  value += '.';
  value += char('0' + tenths % 10);
  return value;
}

/**
 * @brief Update readings.
 * @return No value.
 */
void updateReadings() {
  if (!portal.portalAndAppConfigReady()) return;
  if (isnan(rawTemperature) || isnan(rawHumidity)) {
    updateMeasurement("temperature", nullptr);
    updateMeasurement("humidity", nullptr);
    return;
  }
  float temperature = rawTemperature + portal.getAppConfigValue("temperature_offset").asDouble();
  float humidity = rawHumidity + portal.getAppConfigValue("humidity_offset").asDouble();
  humidity = constrain(humidity, 0.0f, 100.0f);
  updateMeasurement("temperature", roundedTenths(temperature) / 10.0);
  updateMeasurement("humidity", roundedTenths(humidity) / 10.0);
}

unsigned registeredPages = 0;
bool applicationReady = false;

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  portal.onAppConfigPageRegistrationFinished([](bool success) {
    if (!success) {
      logDiagnostic(portal.appConfigRegistrationError().reason);
      return;
    }
    ++registeredPages;
    if (registeredPages == 1 && !registerSensorPage())
      logDiagnostic(portal.appConfigRegistrationError().reason);
  });
  if (!portal.startAppConfigPageRegistration(FPSTR(HOME_PAGE)))
    logDiagnostic(portal.appConfigRegistrationError().reason);
  portal.onAppConfigValueChanged([](const String& key, const ArdJSON::JSONVar&,
                                    ArdPortal::ChangeSource) {
    if (key == "dht_pin" || key == "dht_type") sensorSettingsChanged = true;
    if (key == "mqtt_update") loadMqttInterval();
    if (key == "temperature_offset" || key == "humidity_offset") {
      updateReadings();                   // Reuse the latest sample without another DHT transaction.
      measurementSettingsChanged = true;  // Publish even when a long MQTT interval is selected.
    }
  });
  ArdPortal::Options options;
  options.appStateIntervalMs = 0;  // The example owns periodic measurement refresh.
  if (!portal.begin(options)) logDiagnostic(F("Could not start portal."));
}

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {
  portal.loop();
  uint32_t now = millis();
  if (!portal.portalAndAppConfigReady() || registeredPages != 2 || portal.ota().active()) return;
  if (!applicationReady) {
    applicationReady = true;
    loadMqttInterval();
    sensorSettingsChanged = true;
  }
  if (sensorSettingsChanged) {
    configureSensor();
    now = millis();
  }
  if (uint32_t(now - lastRead) >= READ_INTERVAL_MS) {
    lastRead = now;
    rawTemperature = dht.readTemperature();
    rawHumidity = dht.readHumidity();  // Uses the same cached DHT transaction.
    if (isnan(rawTemperature) || isnan(rawHumidity)) {
      if (!failedReads) {
        logDiagnostic(String(F("DHT: read failed; idle DATA=")) + (digitalRead(activeDhtPin) == HIGH ? F("HIGH") : F("LOW")));
      }
      firstReadingPending = true;  // Log recovery even after the retry counter is reset.
      if (++failedReads >= 3) {
        dht.begin();
        failedReads = 0;
        lastRead = millis();
        logDiagnostic(F("DHT: host driver reinitialized; retry in two seconds (sensor power unchanged)."));
      }
    } else {
      if (firstReadingPending || failedReads) {
        logDiagnostic(String(F("DHT: reading OK; temperature=")) + oneDecimal(rawTemperature) + F(" C, humidity=") + oneDecimal(rawHumidity) + F(" %"));
        firstReadingPending = false;
      }
      failedReads = 0;
    }
    updateReadings();
  }
  serviceMqttUpdates(now);
}
