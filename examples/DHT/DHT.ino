// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * DHT on GPIO2 with a small Wi-Fi portal, MQTT, Home Assistant, Console and OTA.
 * Home shows readings; Settings contains settings. Six fields: temperature, humidity, C/F, two offsets and MQTT update rate.
 * Install Adafruit's DHT sensor library and its Unified Sensor dependency.
 * Connect VCC to 3.3 V, GND to GND and DATA to GPIO2 with a 4.7-10 kOhm
 * pull-up to 3.3 V. GPIO2 must remain high during ESP8266 boot (not D2/GPIO4).
 * MQTT TLS, Console, dependent fields and unused controls are compiled out.
 * Readings update RAM only; units and calibration are saved by ArdFS.
 * DHT reads are synchronous (~25 ms normally), scheduled every two seconds.
 * HA temperature stays numeric in Celsius; its UI handles unit conversion.
 */
#define ARDPORTAL_ENABLE_OTA 1
#define ARDPORTAL_ENABLE_MQTT 1
#define ARDPORTAL_ENABLE_MQTT_TLS 0
#define ARDPORTAL_ENABLE_HA 1
#define ARDPORTAL_ENABLE_CONSOLE 1
#define ARDPORTAL_ENABLE_DEPENDENCIES 0
#define ARDPORTAL_ENABLE_CONTROLS 0
#define ARDPORTAL_ENABLE_CONTROL_SLIDER 1
#define ARDPORTAL_ENABLE_CONTROL_TEXT 1
#define ARDPORTAL_ENABLE_CONTROL_SELECT 1
#include <ArdPortal.h>
#include <DHT.h>

constexpr uint8_t DHT_PIN = 2;
constexpr uint32_t READ_INTERVAL_MS = 2000;
ArdPortal portal;
DHT dht(DHT_PIN, DHT22);
float rawTemperature = NAN, rawHumidity = NAN;
uint32_t lastRead = 0, lastMqttUpdate = 0, mqttIntervalMs = 0;
bool readingsChanged = false, measurementSettingsChanged = false;
uint8_t failedReads = 0;

static const char HOME_PAGE[] PROGMEM = R"JSON({
  "id":"home","name":"Home","order":0,"fields":[
    {"id":"temperature","type":"text","name":"Temperature","default":"Unavailable","persist":false,
     "device_class":"temperature","unit_of_measurement":"°C","state_class":"measurement",
     "ha":{"value_template":"{% set n=value.split()[0]|float(none) %}{{ '' if n is none else ((n-32)*5/9)|round(1) if value.endswith('F') else n }}"}},
    {"id":"humidity","type":"text","name":"Humidity (%)","default":"Unavailable","persist":false,
     "device_class":"humidity","unit_of_measurement":"%","state_class":"measurement"}
  ]
})JSON";

static const char SENSOR_PAGE[] PROGMEM = R"JSON({
  "id":"settings","name":"Settings","order":10,"fields":[
    {"id":"units","type":"select","name":"Temperature units","default":"C",
     "options":[{"value":"C","name":"Celsius (°C)"},{"value":"F","name":"Fahrenheit (°F)"}]},
    {"id":"temperature_offset","type":"slider","name":"Temperature offset (°)","min":-10,"max":10,"step":0.1,"default":0},
    {"id":"humidity_offset","type":"slider","name":"Humidity offset (%)","min":-20,"max":20,"step":1,"default":0},
    {"id":"mqtt_update","type":"select","name":"MQTT updates","default":"0",
     "options":[{"value":"0","name":"On reading change"},{"value":"10","name":"Every 10 seconds"},
                {"value":"60","name":"Every minute"},{"value":"600","name":"Every 10 minutes"},
                {"value":"3600","name":"Every hour"}]}
  ]
})JSON";

/**
 * @brief Update measurement.
 * @param key Configuration key or JSON object member name.
 * @param value Input value, or output destination when passed by mutable reference.
 * @return No value.
 */
void updateMeasurement(const char* key, const ArdJSON::JSONVar& value) {
  readingsChanged |= ArdJSON::JSON.stringify(portal.getAppConfigValue(key)) != ArdJSON::JSON.stringify(value);
  // Live portal updates are separate from the MQTT publishing schedule.
  if (!portal.setAppConfigStateValue(key,value,false)) Serial.println(F("DHT: portal value rejected."));
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
  if(!portal.mqttConnected() || portal.ota().active()) return;
  if(!measurementSettingsChanged && (mqttIntervalMs ? uint32_t(now-lastMqttUpdate)<mqttIntervalMs : !readingsChanged)) return;
  // Only queue state: the portal publishes one field at a time and yields.
  if(portal.queueAppConfigStatePublish("temperature") && portal.queueAppConfigStatePublish("humidity")) {
    lastMqttUpdate=now;readingsChanged=false;measurementSettingsChanged=false;
  }
}

/**
 * @brief Round a sensor value to one decimal place.
 * @param number Sensor reading to round.
 * @return The reading rounded to one decimal place.
 */
String oneDecimal(float number) {
  int tenths = int(number * 10 + (number < 0 ? -0.5f : 0.5f));
  String value;
  if (tenths < 0) { value += '-'; tenths = -tenths; }
  value += String(tenths / 10); value += '.'; value += char('0' + tenths % 10);
  return value;
}

/**
 * @brief Update readings.
 * @return No value.
 */
void updateReadings() {
  if (!portal.portalAndAppConfigReady()) return;
  if (isnan(rawTemperature) || isnan(rawHumidity)) {
    updateMeasurement("temperature", "Unavailable");
    updateMeasurement("humidity", "Unavailable");
    return;
  }
  float temperature = rawTemperature + portal.getAppConfigValue("temperature_offset").asDouble();
  float humidity = rawHumidity + portal.getAppConfigValue("humidity_offset").asDouble();
  humidity = constrain(humidity, 0.0f, 100.0f);
  bool fahrenheit = portal.getAppConfigValue("units").asString() == "F";
  if (fahrenheit) temperature = temperature * 1.8f + 32.0f;
  // One fixed decimal avoids linking the general float-to-string formatter.
  String value = oneDecimal(temperature);
  value += fahrenheit ? F(" °F") : F(" °C");
  updateMeasurement("temperature", value);
  updateMeasurement("humidity", oneDecimal(humidity));
}

unsigned registeredPages = 0;
bool applicationReady = false;

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  dht.begin();
  portal.onAppConfigPageRegistrationFinished([](bool success) {
    if (!success) { Serial.println(portal.appConfigRegistrationError().reason); return; }
    ++registeredPages;
    if (registeredPages == 1 && !portal.startAppConfigPageRegistration(FPSTR(SENSOR_PAGE)))
      Serial.println(portal.appConfigRegistrationError().reason);
  });
  if (!portal.startAppConfigPageRegistration(FPSTR(HOME_PAGE)))
    Serial.println(portal.appConfigRegistrationError().reason);
  portal.onAppConfigValueChanged([](const String& key, const ArdJSON::JSONVar&,
                                   ArdPortal::ChangeSource) {
    if (key == "mqtt_update") loadMqttInterval();
    if (key == "units" || key == "temperature_offset" || key == "humidity_offset") {
      updateReadings(); // Reuse the latest sample without another DHT transaction.
      measurementSettingsChanged=true; // Publish even when a long MQTT interval is selected.
    }
  });
  ArdPortal::Options options;
  options.appStateIntervalMs=0; // The example owns periodic measurement refresh.
  if (!portal.begin(options)) Serial.println(F("Could not start portal."));
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
    lastRead = now; // Wait two seconds before the first sensor transaction.
  }
  if(uint32_t(now-lastRead)>=READ_INTERVAL_MS) {
    lastRead = now;
    rawTemperature = dht.readTemperature();
    rawHumidity = dht.readHumidity(); // Uses the same cached DHT transaction.
    if (isnan(rawTemperature) || isnan(rawHumidity)) {
      if (!failedReads) Serial.println(F("DHT: read failed."));
      if (++failedReads >= 3) {
        dht.begin();failedReads=0;lastRead=millis();
        Serial.println(F("DHT: reinitialized; retry in two seconds."));
      }
    } else {
      if (failedReads) Serial.println(F("DHT: readings recovered."));
      failedReads=0;
    }
    updateReadings();
  }
  serviceMqttUpdates(now);
}
