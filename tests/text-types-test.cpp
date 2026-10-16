#include <cassert>
#include "ZigbeeFields.h"
uint32_t fakeMillis=0;
/** @brief Check borrowed strings and optional text types. @return Zero when all assertions pass. */
int main(){
 using V=ArdJSON::JSONVar;
 V field=ArdJSON::JSON.parse("{\"type\":\"text\"}");
 assert(ArdText::pattern(field).type==ArdText::Type::Text);
 assert(ArdText::validValue(field,"ready")&&ArdText::validValue(field,42));
 V text="ready",number=42;
 assert(text.stringValue()&&*text.stringValue()=="ready"&&!number.stringValue());
 const String* borrowed=text.stringValue();assert(borrowed==text.stringValue());
 const char* types[]={"number","temperature","humidity","pressure","illuminance","flow","carbon_dioxide","pm25","wind_speed","voltage","current","power"};
 for(const auto* type:types){
  field["textType"]=type;V discovery=V::object();ArdText::haPattern(field,discovery);
#if ARDPORTAL_ENABLE_TEXT_TYPES
  assert(ArdText::pattern(field).type!=ArdText::Type::Invalid);
  assert(ArdText::validValue(field,12)&&ArdText::validValue(field,nullptr)&&!ArdText::validValue(field,"bad"));
  assert(discovery["state_class"].asString()=="measurement");
  assert(ArdZigbeeFields::describe(field,12).kind!=ArdZigbeeFields::Kind::None);
#else
  assert(ArdText::pattern(field).type==ArdText::Type::Invalid);
  assert(!ArdText::validValue(field,12)&&!ArdText::validValue(field,nullptr));
  assert(discovery.length()==0&&ArdZigbeeFields::describe(field,12).kind==ArdZigbeeFields::Kind::None);
#endif
 }
 field["textType"]="text";assert(ArdText::validValue(field,"ready"));
 field["textType"]=true;assert(ArdText::pattern(field).type==ArdText::Type::Invalid);
 field["textType"]="invalid";assert(ArdText::pattern(field).type==ArdText::Type::Invalid);
}
