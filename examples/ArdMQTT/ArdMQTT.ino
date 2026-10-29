// Standalone MQTT client, without ArdPortal. Replace Wi-Fi and broker settings.
#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <ArdMQTT.h>

WiFiClient transport;
ArdMQTT mqtt(transport);
uint32_t retrySince = 0;
bool subscribed = false;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
  mqtt.setCallback([](char* topic, uint8_t* payload, unsigned int length) {
    Serial.print(topic);
    Serial.print(": ");
    Serial.write(payload, length);
    Serial.println();
  });
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    mqtt.disconnect();
    subscribed = false;
    return;
  }
  if (!mqtt.connected() && !mqtt.pending() && uint32_t(millis() - retrySince) >= 5000) {
    retrySince = millis();
    subscribed = false;
    if (transport.connect("broker.local", 1883)) mqtt.connect("ardmqtt-example");
  }
  mqtt.loop();
  if (mqtt.connected() && !subscribed) subscribed = mqtt.subscribe("example/#");
}
