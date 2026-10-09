#include <cassert>
#define ARDPORTAL_TEST_ALLOCATE testAllocate
#include "TopicTemplates.h"
static long allocationBudget=-1;
void* testAllocate(size_t bytes) noexcept { if(allocationBudget==0)return nullptr;if(allocationBudget>0)--allocationBudget;return std::malloc(bytes); }
uint32_t fakeMillis=0;
using V=ArdJSON::JSONVar;
int main(){
 V f=ArdJSON::JSON.parse(R"({"id":"x","name":"$device label","ha":{"state_topic":"stat/$device/x","command_topic":"cmnd/$device/$device","value_template":"{{ '$device' }}","payload_on":"$device"}})");
 assert(ArdTopicTemplates::resolve(f,"unit-abc"));const V& c=f;
 assert(c["ha"]["state_topic"].asString()=="stat/unit-abc/x");assert(c["ha"]["command_topic"].asString()=="cmnd/unit-abc/unit-abc");
 assert(c["ha"]["value_template"].asString()=="{{ '$device' }}");assert(c["ha"]["payload_on"].asString()=="$device");assert(c["name"].asString()=="$device label");
 assert(ArdTopicTemplates::resolve(f,"changed"));assert(c["ha"]["state_topic"].asString()=="stat/unit-abc/x");
 V plain=ArdJSON::JSON.parse(R"({"type":"slider","id":"a"})");assert(ArdTopicTemplates::resolve(plain,""));assert(!plain.hasOwnProperty("ha"));
 V unresolved=ArdJSON::JSON.parse(R"({"ha":{"state_topic":"stat/$device/a"}})");assert(!ArdTopicTemplates::resolve(unresolved,""));
 V oom=ArdJSON::JSON.parse(R"({"ha":{"state_topic":"stat/$device/a"}})");
 allocationBudget=0;assert(!ArdTopicTemplates::resolve(oom,"test"));allocationBudget=-1;
 assert(ArdTopicTemplates::resolve(oom,"test"));
 const char* examples[]={
 R"({"id":"x","name":"$device label","ha":{"state_topic":"stat/$device/x","command_topic":"cmnd/$device/$device","value_template":"{{ '$device' }}","payload_on":"$device","nested":{"topic":"$device"},"topic":123}})",
 R"({"ha":{"st\u0061te_topic":"$device\u002fxyz","topic":"$device"},"label":"escaped \"$device\""})",
 R"({"ha":null,"type":"text","default":"$device"})",
 R"({"id":"x","ha":{"state_topic":"stat/plain"}})"};
 for(const char* input:examples){
  V expected=ArdJSON::JSON.parse(input);assert(ArdTopicTemplates::resolve(expected,"unit-abc"));
  String text=ArdTopicTemplates::resolveJson(input,"unit-abc");assert(text.length());
  V actual=ArdJSON::JSON.parse(text);assert(actual.isValid());
  assert(ArdJSON::JSON.stringify(expected)==ArdJSON::JSON.stringify(actual));
 }
 assert(!ArdTopicTemplates::resolveJson(R"({"ha":{"topic":"$device"}})","").length());
 assert(ArdTopicTemplates::resolveJson(R"({"ha":{"topic":"plain"}})","").length());
 assert(!ArdTopicTemplates::resolveJson(R"({"ha":{"topic":"plain"},})","").length());
 assert(!ArdTopicTemplates::resolveJson(R"({"ha":{"topic":"plain"})","").length());
 allocationBudget=0;assert(ArdTopicTemplates::resolveJson(R"({"ha":{"topic":"$device"}})","unit").length());allocationBudget=-1;
 V extended=ArdJSON::JSON.parse(ArdTopicTemplates::fieldHttpJson(R"({"id":"t","type":"time","extended":false,"persist":false,"default":"08:00","ha":{"state_topic":"stat/$device/t"}})","unit",true,true));
 assert(extended.isValid()&&extended["extended"].asBool()&&extended["persist"].asBool()&&extended["default"].asString()=="08:00");assert(extended["ha"]["state_topic"].asString()=="stat/unit/t");
 puts("PASS: topic expansion, multiple tokens, literal labels/payloads/templates, no mutation of plain fields, missing identity rejection");
}
