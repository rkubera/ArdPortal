// SPDX-License-Identifier: MIT
#include <ArdPortal.h>

ArdPortal portal;
constexpr uint8_t WakePin=4; // Button to GND; choose a suitable pin for your board.

void onPowerEvent(const ArdPower::Event& event,void*) {
  // Called from portal.loop(), never from an ISR. Keep work cooperative.
  if(event.kind==ArdPower::EventKind::Timer) {
    Serial.println(F("Periodic sensor reading"));
    // Read sensors and update application state here.
  }else if(event.kind==ArdPower::EventKind::Gpio) {
    Serial.println(F("Button event"));
  }else {
    Serial.println(F("External ISR event"));
  }
}

// An application-owned ISR can call this API without allocation or callbacks.
// Configure its hardware wake source separately if it must wake a sleeping CPU.
void IRAM_ATTR dataReadyISR() {
  portal.power().signalFromISR(0);
}

void setup() {
  Serial.begin(115200);
  ArdPower::Options options;
  options.policy=ArdPower::Policy::EventDriven;
  options.sleep=ArdPower::SleepType::ConnectedSleep;
  options.connectedSleep.allowCpuSleep=true; // False saves only radio power.
  options.sleepDelayMs=0;
  options.maxIdleMs=20;
  portal.power().configure(options);
  portal.power().onWake(onPowerEvent);
  pinMode(WakePin,INPUT_PULLUP);
  if(!portal.power().addWakePin(WakePin,ArdPower::WakeLevel::Low))
    Serial.println(F("Wake pin is unsupported"));
  portal.power().setTimer(1,10000,true); // Repeating software timer; ID 1.
  portal.power().setTimer(2,3000);      // One-shot software timer; ID 2.
  portal.begin();
}

void loop() {
  portal.loop();
  // CPU/deep sleep waits while the provisioning AP or a WebSocket is active.
  // ESP32: use WakeScope::LightAndDeepSleep when registering a deep-wake pin.
  // ESP8266: arbitrary GPIO deep wake is unsupported; timer wake needs GPIO16->RST.
  // Software timers do not survive a deep-sleep restart.
}
