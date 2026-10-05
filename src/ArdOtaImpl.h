// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#include "ArdPortalFeatures.h"
#if ARDPORTAL_ENABLE_OTA
#include "ArdOta.h"
#include "ArdPortalDeclarations.h"
#include "Language.h"
#if defined(ESP32)
#include <Update.h>
#else
#include <Updater.h>
#endif

void ArdOta::abortUpgrade() {
#if defined(ESP32)
  Update.abort();
#else
  Update.end(false); // Incomplete image is rejected; ESP8266 has no public abort().
#endif
  _otaActive = false;
}

bool ArdOta::writeUpgrade(uint8_t* data, size_t length) {
  if (!_otaReceived && length && data[0] != 0xe9) { abortUpgrade(); _portal.replyMessage(400,ArdUILanguage::Key::s_152); return false; }
  size_t written = Update.write(data, length);
  if (written != length) { abortUpgrade(); _portal.reply(500, "text/plain", ArdUILanguage::text(ArdUILanguage::Key::s_153) + String(Update.getError())); return false; }
  _otaReceived += length;
  if (_otaReceived == _otaExpected) {
    bool ok = Update.end(false); _otaActive = false;
    if (!ok) { _portal.reply(500, "text/plain", ArdUILanguage::text(ArdUILanguage::Key::s_153) + String(Update.getError())); return false; }
    _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_155));
    _portal._rebootPending = true; _portal._rebootSince = millis();
    _portal.replyMessage(200,ArdUILanguage::Key::s_155);
  }
  return true;
}

void ArdOta::startUpload(uint32_t bodySize, int split) {
    if (!_portal._configurationReady || _portal.storageBusy() || _portal.mqttTrialActive() || _portal._rebootPending || _portal._scanning) { _portal.replyMessage(409,ArdUILanguage::Key::s_132); return; }
    if (!bodySize || bodySize > ESP.getFreeSketchSpace()) { _portal.replyMessage(413,ArdUILanguage::Key::s_116); return; }
    _portal.closeMqtt();
    _portal.log(ArdUILanguage::text(ArdUILanguage::Key::s_117));
    if (!Update.begin(bodySize, U_FLASH)) { _portal.reply(500, "text/plain", ArdUILanguage::text(ArdUILanguage::Key::s_153) + String(Update.getError())); return; }
    _otaActive = true; _otaExpected = bodySize; _otaReceived = 0; _portal._httpSince = millis();
    String initial = _portal._request.substring(split + 4); _portal._request = String();
    size_t length = initial.length(); if (length > bodySize) length = bodySize;
    if (length) writeUpgrade(reinterpret_cast<uint8_t*>(const_cast<char*>(initial.c_str())), length);
    return;
}

void ArdOta::receiveUpload() {
    uint8_t data[512]; size_t count = 0;
    while (count < sizeof(data) && count < _otaExpected - _otaReceived && _portal._http.available()) {
      int c = _portal._http.read(); if (c < 0) break; data[count++] = c;
    }
    if (count) { _portal._httpSince = millis(); writeUpgrade(data, count); }
    return;
}

#endif
