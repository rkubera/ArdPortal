#include "ZigbeeFields.h"
#define private public
#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 1
#define ARDPORTAL_ZIGBEE_ROUTER 0
#include "ArdZigbeeImpl.h"
#undef private
#include <cassert>
uint32_t fakeMillis=0;
time_t fakeUnixTime=0;
EspStub ESP;
SerialStub Serial;
using namespace ArdZigbeeFields;
/** @brief Exercise actual SDK adapter with controlled allocation, callbacks and transport failures. @return Zero on success. */
int main(){
 assert(uint8_t(ArdZigbeeIdentity::Model[0])==12);
#if defined(CONFIG_IDF_TARGET_ESP32C5)
 assert(strcmp(ArdZigbeeIdentity::Model+1,"ArdPortal-C5")==0);
#else
 assert(strcmp(ArdZigbeeIdentity::Model+1,"ArdPortal-C6")==0);
#endif

 ArdZigbee z;
#if ARDPORTAL_ZIGBEE_DEBUG_SERIAL
 z.log("Serial diagnostic");assert(Serial.output.find("Serial diagnostic")!=std::string::npos);
 auto serialBefore=Serial.output;Serial.capacity=0;z.log("Console only");assert(Serial.output==serialBefore);assert(z.nextLine(1));Serial.capacity=256;
#endif
 Spec spec;spec.kind=Kind::Switch;spec.field=7;V f=V::object();auto initial=encode(spec,f,false);
 assert(z.addControl(spec,initial,"Relay"));assert(z.controls()->spec.endpoint==2);
 V field=ArdJSON::JSON.parse(R"({"options":[{"value":"a"},{"value":"b"}]})");
 V state=ArdJSON::JSON.parse(R"({"state":"ON","brightness":255,"color":{"r":255,"g":0,"b":0},"temperature":22,"current_temperature":20,"mode":"heat","fan_mode":"auto","position":20,"tilt":70})");
 for(auto kind:{Kind::Action,Kind::State,Kind::Slider,Kind::Select,Kind::Number,Kind::Temperature,Kind::Humidity,Kind::Pressure,Kind::Illuminance,Kind::Flow,Kind::WindSpeed,Kind::CarbonDioxide,Kind::PM25,Kind::Voltage,Kind::Current,Kind::Power,Kind::Binary,Kind::Light,Kind::Climate,Kind::Cover,Kind::Lock,Kind::Siren}){
   Spec mapping;mapping.kind=kind;mapping.min=10;mapping.max=30;mapping.step=0.5;mapping.states=2;mapping.fan=true;
   V value=kind==Kind::Select?V("a"):kind==Kind::Lock?V("LOCKED"):kind==Kind::Binary?V(true):kind==Kind::Light||kind==Kind::Climate||kind==Kind::Cover||kind==Kind::Siren?state:V(22);
   auto attributes=encode(mapping,field,value);assert(z.addControl(mapping,attributes,kind==Kind::Number?String():String("Native")));
 }
 assert(z.controlCount()==23);nativeFailure=true;assert(!z.createControls(&nativeEndpoints));nativeFailure=false;assert(z.createControls(&nativeEndpoints));assert(!z.controls()->initial&&!z.controls()->label);
 float maximum=0,minimum=0;auto* slider=nativeEndpoints.endpoints[5]->clusters[13];memcpy(&maximum,slider->attributes[0x41].bytes.data(),4);memcpy(&minimum,slider->attributes[0x45].bytes.data(),4);assert(maximum==30&&minimum==10);assert(!z._startupError[0]);
 assert(z.begin(ArdZigbee::Options()));assert(!z.addControl(spec,initial,"Late"));z._ready=true;z._state=ArdZigbee::State::Connected;
 auto values=encode(spec,f,true);assert(z.publish(2,values));assert(reports==1);assert(z.publish(2,values)&&reports==1);assert(z.publish(2,values,true)&&reports==2);
 reportFailure=true;assert(!z.publish(2,values,true));reportFailure=false;assert(z.publish(2,values,true));

 auto* light=z._controls.get();while(light&&light->spec.kind!=Kind::Light)light=light->next.get();assert(light);
 auto lightValues=encode(light->spec,field,state);
 auto* mode=esp_zb_zcl_get_attribute(light->spec.endpoint,0x300,0,8);mode->access=ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY;
 unsigned before=reports;failedReportCluster=0x300;failedReportAttribute=1;
 for(unsigned i=1;i<=3;++i){assert(!z.publish(light->spec.endpoint,lightValues,true)&&reports==before+i&&light->reportIncomplete);}
 assert(!z.publish(light->spec.endpoint,lightValues,true)&&reports==before+4&&!light->reportIncomplete);
 assert(!z.publish(light->spec.endpoint,lightValues,true)&&reports==before+5);
 failedReportCluster=failedReportAttribute=-1;
 for(unsigned i=6;i<8;++i)assert(!z.publish(light->spec.endpoint,lightValues,true)&&reports==before+i&&light->reportIncomplete);
 assert(z.publish(light->spec.endpoint,lightValues,true)&&reports==before+8);
 assert(z.publish(light->spec.endpoint,lightValues)&&reports==before+8);
 assert(!light->pendingReports);
 lockAvailable=false;assert(!z.publish(2,values,true));lockAvailable=true;

 z.queueReports(size_t(-1),true);assert(z.nextReport(100,0)->spec.endpoint==2);
 z.finishReport(2,false,100);assert(z.nextReport(101,0)->spec.endpoint==3);
 z.finishReport(3,true,101);assert(z.nextReport(102,2)->spec.endpoint==4);
 assert(z.nextReport(5099,0)->spec.endpoint!=2);assert(z.nextReport(5100,0)->spec.endpoint==2);
 z.queueReports(7,true);assert(z.nextReport(103,0)->spec.endpoint==2);
 uint8_t data=1;esp_zb_zcl_set_attr_value_message_t msg{};msg.info.status=0;msg.info.dst_endpoint=2;msg.info.cluster=6;msg.attribute.id=0;msg.attribute.data={0x10,1,&data};
 assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID,&msg)==ESP_OK);Command c;assert(z.nextCommand(c)&&c.endpoint==2&&c.value==1&&c.cluster==6);
 char payload[32];float decimal=20.5f;ardZigbeePayload(payload,sizeof(payload),&decimal,0x39,4);assert(std::string(payload)=="20.5");
 int16_t negative=-125;ardZigbeePayload(payload,sizeof(payload),&negative,0x29,2);assert(std::string(payload)=="-125");
 ardZigbeePayload(payload,sizeof(payload),&data,0x10,1);assert(std::string(payload)=="true");
 for(unsigned i=0;i<8;++i)z.loop(fakeMillis);
 bool sawPayload=false;for(uint32_t cursor=0;const auto* line=z.nextLine(cursor);){cursor=line->id;if(line->text.indexOf("RX applied ep=2 payload={\"cluster\":6,\"attr\":0,\"type\":16,\"value\":true}")>=0)sawPayload=true;}assert(sawPayload);
 unsigned lifecycleLogs=0;z.onLifecycleDiagnostic(&lifecycleLogs,[](void* owner,const String&){++*static_cast<unsigned*>(owner);});
 z.log("Zigbee link lost");assert(lifecycleLogs==1);
 z.traffic("RX payload={\"cmd\":\"on\"}");for(unsigned i=0;i<8;++i)z.loop(fakeMillis);assert(lifecycleLogs==1);
 z.onLifecycleDiagnostic(nullptr,nullptr);

 uint16_t enhancedHue=uint16_t(std::lround(210.0*65535/360));esp_zb_zcl_set_attr_value_message_t colorMsg{};colorMsg.info.status=0;colorMsg.info.dst_endpoint=light->spec.endpoint;colorMsg.info.cluster=0x300;colorMsg.attribute.id=0x4000;colorMsg.attribute.data={0x21,2,&enhancedHue};
 assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID,&colorMsg)==ESP_OK);Command colorCommand;assert(z.nextCommand(colorCommand)&&colorCommand.cluster==0x300&&colorCommand.id==0&&colorCommand.hue>=147&&colorCommand.hue<=149);
 msg.attribute.data.type=0x20;assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID,&msg)==ESP_ERR_INVALID_ARG);msg.attribute.data.type=0x10;
 for(unsigned i=0;i<16;++i)assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID,&msg)==ESP_OK);
 assert(ArdZigbee::action(ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID,&msg)==ESP_ERR_NO_MEM);unsigned count=0;while(z.nextCommand(c))++count;assert(count==16);
 puts("PASS: native SDK staging, startup-buffer release, late registration, state reports, retry/defer, typed commands and queue saturation");
}
