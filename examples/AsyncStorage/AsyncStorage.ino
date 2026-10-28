// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT

/*
 * AsyncStorage
 *
 * Save and read a JSON configuration with ArdFS, without starting ArdPortal.
 * writeJsonConfig()/readJsonConfig() use bounded slices, alternating copies,
 * checksums and cooperative loop() steps. Identical saves are skipped.
 *
 * Requirements: ESP8266/ESP32 with an ArdFS region; Serial at 115200 baud.
 * Do not share the volume with a running portal or another storage instance.
 */
#include <ArdFS.h>

ArdFS storage;
bool started = false;
// The immutable producer remains available until the save callback completes.
static const char configJson[] = "{\"enabled\":true}";

void setup() {
  Serial.begin(115200);
  storage.begin(); // Mount/initial format advances from loop().
}

void loop() {
  storage.loop();
  if (started || !storage.ready()) return;
  started = true;
  if (!storage.mounted()) {
    Serial.println(storage.error());
    return;
  }
  if (!storage.writeJsonConfig("/example.json", sizeof(configJson) - 1,
        [](size_t offset, size_t maximum) {
          String part;
          if (!part.reserve(maximum) || !part.concat(configJson + offset, maximum)) return String();
          return part;
        },
        [](const ArdFS::Result& saved) {
          if (!saved.ok) { Serial.println(saved.error); return; }
          Serial.println(saved.changed ? "Saved." : "Identical data: write skipped.");
          if (!storage.readJsonConfig("/example.json",
                [](size_t offset, const uint8_t* bytes, size_t length) {
                  if (!offset) Serial.print("JSON: ");
                  Serial.write(bytes, length); // Consume slices without a full String.
                  return true;
                },
                [](const ArdFS::Result& loaded) {
                  Serial.println();
                  if (!loaded.ok) Serial.println(loaded.error);
                  else if (!loaded.found) Serial.println("No saved configuration.");
                }, true)) Serial.println("Read was not accepted.");
        })) Serial.println("Write was not accepted.");
}
