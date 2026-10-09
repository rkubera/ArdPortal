// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// Build with compiler.c.elf.extra_flags=-Wl,--wrap=malloc (see README).
#if !defined(ESP32)
#error This allocation-failure regression requires ESP32
#endif
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdlib>
#include <ArdPortal.h>

extern "C" void* __real_malloc(size_t bytes);
static volatile bool rejectNext=false;
static TaskHandle_t rejectionTask=nullptr;
static volatile unsigned rejected=0;

/**
 * @brief Deny one allocation from the test task while leaving other ESP32 tasks unaffected.
 * @param bytes Requested allocation size.
 * @return nullptr for the forced failure; otherwise the real allocator result.
 */
extern "C" void* __wrap_malloc(size_t bytes) {
  if(rejectNext&&xTaskGetCurrentTaskHandle()==rejectionTask){rejectNext=false;++rejected;return nullptr;}
  return __real_malloc(bytes);
}

ArdPortal portal;
WiFiServer probe(8080);
static uint32_t ticks=0,lastReport=0;
static bool failuresPassed=false,runtimeChecked=false;

/**
 * @brief Force JSON object-member and traversal-array allocation failures, then verify recovery.
 * @return True if both paths returned errors and a subsequent JSON parse succeeded.
 */
bool allocationFailures() {
  rejected=0;rejectionTask=xTaskGetCurrentTaskHandle();
  auto object=ArdJSON::JSONVar::object();
  rejectNext=true;object["id"]=1;
  bool memberFailed=!rejectNext&&!object.isValid();rejectNext=false;
  ArdDependencies::Stack frames;
  rejectNext=true;bool arrayFailed=!frames.reserve(1,1024)&&!rejectNext;rejectNext=false;
  auto recovered=ArdJSON::JSON.parse("{\"id\":1}");
  return memberFailed&&arrayFailed&&recovered.isValid()&&recovered["id"].asDouble()==1&&rejected==2;
}

/**
 * @brief Start the portal and HTTP probe, then exercise refused allocations on the real ESP32 runtime.
 * @return No value.
 */
void setup() {
  Serial.begin(115200);
  failuresPassed=allocationFailures();
  Serial.println(failuresPassed?"PASS: Member and array OOM; JSON recovered":"FAIL: allocation failure regression");
  portal.begin();probe.begin();
}

/**
 * @brief Continue portal HTTP work, echo UART input and expose a probe after the forced failures.
 * @return No value.
 */
void loop() {
  portal.loop();++ticks;
  if(!runtimeChecked&&millis()>=5000){runtimeChecked=true;failuresPassed=allocationFailures()&&failuresPassed;Serial.println(failuresPassed?"PASS: runtime OOM; HTTP/UART continue":"FAIL: runtime allocation refusal");}
  while(Serial.available())Serial.write(Serial.read());
  auto client=probe.accept();
  if(client){
    while(client.available())client.read();
    char body[64];int length=snprintf(body,sizeof(body),"%s loopTicks=%lu\n",failuresPassed?"PASS":"FAIL",(unsigned long)ticks);
    client.printf("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",length);
    client.write(reinterpret_cast<const uint8_t*>(body),size_t(length));client.stop();
  }
  if(uint32_t(millis()-lastReport)>=1000){lastReport=millis();Serial.printf("loop=%lu heap=%u IP=%s AP=%s\n",(unsigned long)ticks,ESP.getFreeHeap(),WiFi.localIP().toString().c_str(),WiFi.softAPIP().toString().c_str());}
}
