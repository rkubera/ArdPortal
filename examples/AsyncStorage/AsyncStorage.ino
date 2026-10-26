// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT

/*
 * AsyncStorage
 *
 * Purpose: Save and read a JSON document using ArdFS without starting ArdPortal.
 *
 * Demonstrates: Cooperative storage.loop(), journal callbacks and identical-write detection.
 *
 * Requirements: ESP8266/ESP32 with an ArdFS region; Serial Monitor at 115200 baud.
 *
 * What to expect: Writes /example.json and reads it back. Do not share the volume with a
 * running portal.
 */

/*
 * Use ArdFS as a standalone cooperative JSON document store with a journal.
 * Demonstrates background reads/writes, callbacks and skipped identical saves.
 */

#include <ArdFS.h>

ArdFS storage;
bool started = false;

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  storage.begin();  // Automatic mount/format will run from loop().
}

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {
  storage.loop();
  if (!started && storage.ready()) {
    started = true;
    if (!storage.mounted()) {
      Serial.println(storage.error());
      return;
    }
    if (!storage.write("/example.json", "{\"enabled\":true}", [](const ArdFS::Result& result) {
          if (!result.ok) {
            Serial.println(result.error);
            return;
          }
          Serial.println(result.changed ? "Saved." : "Identical data: write skipped.");
          storage.read("/example.json", [](const ArdFS::Result& loaded) {
            Serial.println(loaded.ok && loaded.found ? loaded.data : loaded.error);
          });
        })) Serial.println("Write was not accepted.");
  }
}
