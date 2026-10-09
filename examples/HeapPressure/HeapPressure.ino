// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1
#include <ArdPortal.h>
#include <HeapMemory.h>
ArdPortal portal;
void* pressure[384]={};
size_t held=0;
uint32_t lastReport=0;
const char PAGE[] PROGMEM=R"({"id":"pressure","name":"Pressure","order":0,"fields":[{"id":"load","name":"Load","type":"slider","min":0,"max":100,"default":0},{"id":"gate","name":"Gate","type":"switch","default":true},{"id":"detail","name":"Detail","type":"edit","visibleWhen":{"field":"gate","equals":true}}]})";
/** @brief Set a controlled byte-addressable heap pressure level.
 * @param target Desired free-byte pool size; zero releases all held allocations.
 * @return No value. */
void setPressure(size_t target){
  for(size_t i=0;i<held;++i)free(pressure[i]);
  held=0;
  while(target&&held<384&&ArdHeap::sample().free8>target+512){
#if defined(ESP32)
    void* bytes=heap_caps_malloc(512,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
#else
    void* bytes=malloc(512);
#endif
    if(!bytes)break;
    pressure[held++]=bytes;
  }
  Serial.println(ArdHeap::describe(ArdHeap::sample()));
}
/** @brief Start an ordinary portal with registration diagnostics.
 * @return No value. */
void setup(){Serial.begin(115200);portal.onAppConfigPageRegistrationFinished([](bool okay){Serial.printf("registration=%s\n",okay?"completed":"FAILED");});portal.begin();}
/** @brief Exercise registration/Discovery and serve HTTP under selectable pressure.
 * @return No value. */
void loop(){
  portal.loop();
  if(Serial.available()){
    char command=Serial.read();
    if(command=='0')setPressure(0);
    if(command=='1')setPressure(32768);
    if(command=='2')setPressure(24576);
    if(command=='3')setPressure(16384);
    if(command=='4')setPressure(8192);
    if(command=='r'){bool okay=portal.startAppConfigPageRegistration(FPSTR(PAGE));Serial.printf("accepted=%d\n",okay);}
  }
  if(millis()-lastReport>=1000){lastReport=millis();Serial.println(String("IP=")+portal.localIP().toString()+" "+ArdHeap::describe(ArdHeap::sample())+" permit="+String(ArdHeap::permits(4096))+" registered="+String(int(portal.appConfigPageRegistrationState())));}
}
