// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
/*
 * Create and access ArdJSON objects/arrays, then serialize and parse JSON.
 * Results are printed to the serial monitor; no portal is started.
 */

#include <ArdJSON.h>

using ArdJSON::JSON;
using ArdJSON::JSONVar;

/**
 * @brief Initialize the example hardware, callbacks and portal.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);

  JSONVar document;
  document["device"] = "ArdUI";
  document["enabled"] = true;
  document["values"] = JSONVar::array();
  document["values"].push(42);
  document["values"].push(1.25);

  String error;
  String text = JSON.stringify(document, true, &error);
  if (!text.length()) { Serial.println(error); return; }
  Serial.println(text);

  const JSONVar restored = JSON.parse(text, &error);
  if (!restored.isValid()) { Serial.println(error); return; }
  Serial.println(restored["device"].asString());
  Serial.println(int(restored["values"][0]));
}

/**
 * @brief Advance the component work; call repeatedly from the Arduino main loop.
 * @return No value.
 */
void loop() {}
