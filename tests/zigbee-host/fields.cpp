#include <cassert>
#include "ZigbeeFields.h"
uint32_t fakeMillis=0;
using namespace ArdZigbeeFields;
/** @brief Parse a small test definition. @param text JSON. @return Parsed object. */
V json(const char* text){return ArdJSON::JSON.parse(text);}
/** @brief Test native wire conversions and rejection of malformed commands. @return Zero on success. */
int main(){
 V f=json(R"({"type":"switch"})"),next;String action,property;auto s=describe(f,false);auto out=encode(s,f,true);assert(out.count==1&&out.attributes[0].cluster==6&&out.attributes[0].data.u==1);
 Command c;c.cluster=6;c.value=0;assert(decode(s,f,true,c,next,action,property)&&!next.asBool());c.value=2;assert(!decode(s,f,true,c,next,action,property));
 f=json(R"({"type":"slider","min":-10,"max":40,"step":0.1})");s=describe(f,0);c.cluster=0xd;c.id=0x55;float n=22.3;memcpy(&c.value,&n,4);assert(decode(s,f,0,c,next,action,property)&&fabs(next.asDouble()-22.3)<1e-5);n=22.35;memcpy(&c.value,&n,4);assert(!decode(s,f,0,c,next,action,property));n=NAN;memcpy(&c.value,&n,4);assert(!decode(s,f,0,c,next,action,property));
 f=json(R"({"type":"select","options":[{"value":"a"},{"value":"b"}]})");s=describe(f,"b");out=encode(s,f,"b");assert(out.attributes[0].data.u==2);c.cluster=0x13;c.id=0x55;c.value=1;assert(decode(s,f,"b",c,next,action,property)&&next.asString()=="a");c.value=0;assert(!decode(s,f,"b",c,next,action,property));c.value=3;assert(!decode(s,f,"b",c,next,action,property));
 f=json(R"({"type":"text","textType":"temperature","unit_of_measurement":"°C"})");s=describe(f,-12.34);out=encode(s,f,-12.34);assert(s.kind==Kind::Temperature&&int16_t(out.attributes[0].data.u)==-1234);assert(describe(f,nullptr).kind==Kind::Temperature);assert(!ArdText::validValue(f,"hello"));
 f=json(R"({"type":"text","textType":"humidity"})");s=describe(f,50.5);out=encode(s,f,50.5);assert(out.attributes[0].data.u==5050);out=encode(s,f,101);assert(out.attributes[0].data.u==65535);
 f=json(R"({"type":"text"})");assert(describe(f,12).kind==Kind::None);assert(ArdText::validValue(f,"Ready"));f["textType"]="text";assert(describe(f,12).kind==Kind::None);f["textType"]="invalid";assert(ArdText::pattern(f).type==ArdText::Type::Invalid);f["textType"]=true;assert(ArdText::pattern(f).type==ArdText::Type::Invalid);
 for(auto type:{"number","temperature","humidity","pressure","illuminance","flow","wind_speed","carbon_dioxide","pm25","voltage","current","power"}){
   f=json(R"({"type":"text"})");f["textType"]=type;s=describe(f,nullptr);assert(s.kind!=Kind::None);assert(ArdText::validValue(f,nullptr)&&ArdText::validValue(f,12.5)&&!ArdText::validValue(f,"12.5")&&!ArdText::validValue(f,true));
   V ha=V::object();ArdText::haPattern(f,ha);assert(ha["state_class"].asString()=="measurement"&&ha["value_template"].asString().length());
   out=encode(s,f,nullptr);assert(out.count);
 }
 f["textType"]="carbon_dioxide";out=encode(describe(f,0),f,400);assert(out.attributes[0].cluster==0x40d&&fabs(out.attributes[0].data.f-0.0004)<1e-9);out=encode(describe(f,0),f,nullptr);assert(std::isnan(out.attributes[0].data.f));
 f["textType"]="pm25";out=encode(describe(f,0),f,12.5);assert(out.attributes[0].cluster==0x42a&&out.attributes[0].data.f==12.5);
 f["textType"]="illuminance";out=encode(describe(f,0),f,100);assert(out.attributes[0].data.u==20001);out=encode(describe(f,0),f,0);assert(out.attributes[0].data.u==0);out=encode(describe(f,0),f,-1);assert(out.attributes[0].data.u==65535);
 f["textType"]="pressure";out=encode(describe(f,0),f,1013.25);assert(out.attributes[0].data.u==1013);
 f["textType"]="flow";out=encode(describe(f,0),f,12.3);assert(out.attributes[0].data.u==123);
 f["textType"]="wind_speed";out=encode(describe(f,0),f,4.56);assert(out.attributes[0].data.u==456);
 f["textType"]="voltage";out=encode(describe(f,0),f,230.1);assert(out.count==4&&out.attributes[0].type==0x1b&&out.attributes[2].data.u==10&&out.attributes[3].data.u==2301);
 f["textType"]="current";out=encode(describe(f,0),f,1.234);assert(out.attributes[2].data.u==1000&&out.attributes[3].data.u==1234);
 f["textType"]="power";out=encode(describe(f,0),f,-123);assert(int16_t(out.attributes[3].data.u)==-123);
 f=json(R"({"type":"binary_sensor"})");s=describe(f,false);out=encode(s,f,true);assert(out.attributes[0].cluster==0xf&&out.attributes[0].data.u==1);
 f=json(R"({"type":"climate","min":10,"max":30,"step":0.5,"modes":["off","heat","cool","auto"],"fan_modes":["auto","low","high"]})");V current=json(R"({"temperature":22,"current_temperature":null,"mode":"heat","fan_mode":"auto"})");s=describe(f,current);out=encode(s,f,current);assert(out.attributes[0].data.u==0x8000&&out.attributes[1].data.u==3250&&out.attributes[2].data.u==2200&&s.sequence==4);c.cluster=0x201;c.id=0x12;c.value=2350;assert(decode(s,f,current,c,next,action,property)&&next["temperature"].asDouble()==23.5&&next["mode"].asString()=="heat");c.id=0x11;c.value=2600;assert(decode(s,f,current,c,next,action,property)&&next["temperature"].asDouble()==23.5);c.id=0x1c;c.value=3;assert(decode(s,f,current,c,next,action,property)&&next["mode"].asString()=="cool");c.value=7;assert(!decode(s,f,current,c,next,action,property));
 f=json(R"({"type":"light"})");current=json(R"({"state":"ON","brightness":255,"color":{"r":255,"g":0,"b":0}})");s=describe(f,current);out=encode(s,f,current);assert(out.attributes[1].data.u==254&&out.attributes[2].data.u==0&&out.attributes[3].data.u==254);c.cluster=8;c.id=0;c.value=127;assert(decode(s,f,current,c,next,action,property)&&next["brightness"].asDouble()==128);
 current["color"]=json(R"({"r":128,"g":0,"b":0})");out=encode(s,f,current);assert(out.attributes[1].data.u==127);
 c.cluster=0x300;c.id=0;c.value=85;c.hue=85;c.saturation=254;c.level=127;property="";assert(decode(s,f,current,c,next,action,property)&&next["brightness"].asDouble()==128&&next["color"]["g"].asDouble()==255);
 for(auto text:{R"({"r":255,"g":0,"b":0})",R"({"r":0,"g":255,"b":0})",R"({"r":0,"g":0,"b":255})",R"({"r":255,"g":255,"b":255})"}){V rgb=json(text);uint8_t h,sat;rgbToHs(rgb,h,sat);V restored=hsToRgb(h,sat);for(auto key:{"r","g","b"})assert(fabs(rgb[key].asDouble()-restored[key].asDouble())<=3);}
 for(auto text:{R"({"r":255,"g":0,"b":0})",R"({"r":0,"g":255,"b":0})",R"({"r":0,"g":0,"b":255})",R"({"r":255,"g":255,"b":255})",R"({"r":20,"g":200,"b":50})"}){V rgb=json(text);uint16_t x,y;rgbToXy(rgb,x,y);V restored=xyToRgb(x,y);double hi=std::fmax(rgb["r"].asDouble(),std::fmax(rgb["g"].asDouble(),rgb["b"].asDouble()));for(auto key:{"r","g","b"})assert(fabs(rgb[key].asDouble()*255/hi-restored[key].asDouble())<=4);}
 assert(xyToRgb(65535,65535).isUndefined());assert(xyToRgb(0,0).isUndefined());
 c.cluster=0x300;c.id=3;rgbToXy(json(R"({"r":0,"g":255,"b":0})"),c.x,c.y);assert(decode(s,f,current,c,next,action,property)&&next["color"]["g"].asDouble()==255);
 f=json(R"({"type":"climate","min":10,"max":35,"step":0.5,"modes":["off","heat_cool"]})");current=json(R"({"temperature":22,"temperature_low":20,"temperature_high":25,"mode":"heat_cool"})");s=describe(f,current);out=encode(s,f,current);assert(s.sequence==4&&out.attributes[1].data.u==2500&&out.attributes[2].data.u==2000&&out.attributes[3].data.u==1);
 c.cluster=0x201;c.id=0x12;c.value=2150;assert(decode(s,f,current,c,next,action,property)&&next["temperature_low"].asDouble()==21.5&&next["temperature_high"].asDouble()==25);
 c.value=2400;assert(!decode(s,f,current,c,next,action,property));c.value=2125;assert(!decode(s,f,current,c,next,action,property));c.id=0x1c;c.value=1;assert(decode(s,f,current,c,next,action,property)&&next["mode"].asString()=="heat_cool");
 f=json(R"({"type":"cover"})");current=json(R"({"position":25,"tilt":80})");s=describe(f,current);out=encode(s,f,current);assert(out.attributes[0].data.u==75&&out.attributes[1].data.u==20);c.kind=1;c.id=5;c.value=0;assert(decode(s,f,current,c,next,action,property)&&next.asDouble()==100&&property=="position");c.id=2;assert(decode(s,f,current,c,next,action,property)&&action=="STOP");c.id=4;assert(!decode(s,f,current,c,next,action,property));
 f=json(R"({"type":"lock"})");s=describe(f,"LOCKED");out=encode(s,f,"LOCKED");assert(out.attributes[0].data.u==1);c.id=1;assert(decode(s,f,"LOCKED",c,next,action,property)&&action=="UNLOCK");c.id=2;assert(!decode(s,f,"LOCKED",c,next,action,property));
 f=json(R"({"type":"siren"})");s=describe(f,current);c.id=1;c.duration=10;c.volume=2;assert(decode(s,f,current,c,next,action,property)&&next["state"].asString()=="ON"&&next["volume_level"].asDouble()==0.7);
 for(auto type:{"edit","date","time","datetime","notify","infrared","tag","vacuum","fan"}){f=V::object();f["type"]=type;assert(describe(f,0).kind==Kind::None);}
 // Composite control projections preserve their original property and command semantics.
 f=json(R"({"type":"fan","controls":[{"key":"state","type":"switch","on":"ON","off":"OFF"},{"key":"percentage","type":"slider","min":0,"max":100,"step":1},{"key":"direction","type":"select","options":["forward","reverse"]},{"key":"","type":"action","payload":"START"}]})");
 current=json(R"({"state":"ON","percentage":25,"direction":"reverse"})");
 s=describePart(f,0);out=encode(s,f,current);assert(out.attributes[0].data.u==1);c={};c.cluster=6;c.value=0;assert(decode(s,f,current,c,next,action,property)&&next.asString()=="OFF"&&property=="state");
 s=describePart(f,1);out=encode(s,f,current);assert(out.attributes[0].data.f==25);c.cluster=0xd;c.id=0x55;n=70;memcpy(&c.value,&n,4);assert(decode(s,f,current,c,next,action,property)&&next.asDouble()==70&&property=="percentage");n=101;memcpy(&c.value,&n,4);assert(!decode(s,f,current,c,next,action,property));
 s=describePart(f,2);out=encode(s,f,current);assert(out.attributes[0].data.u==2);c.cluster=0x13;c.id=0x55;c.value=1;assert(decode(s,f,current,c,next,action,property)&&next.asString()=="forward"&&property=="direction");
 s=describePart(f,3);out=encode(s,f,current);assert(out.attributes[0].data.u==0);c={};c.cluster=6;c.value=1;assert(decode(s,f,current,c,next,action,property)&&action=="START");assert(decode(s,f,current,c,next,action,property));c.value=0;assert(!decode(s,f,current,c,next,action,property));
 f=json(R"({"type":"valve"})");s.kind=Kind::State;s.part=0xfffe;s.states=stateCount(f);assert(s.states==4);out=encode(s,f,json(R"({"state":"closing"})"));assert(out.attributes[0].data.u==4);assert(!decode(s,f,current,c,next,action,property));
 for(auto type:{"fan","valve","humidifier","water_heater","alarm_control_panel","vacuum","lawn_mower","button","scene"}){f["type"]=type;assert(composite(f));}
 f["type"]="infrared";assert(!composite(f));
 assert(mode("fan_only")==7&&mode("dry")==8&&mode("auto",true)==5);
 for(auto type:{"edit","date","time","datetime","event","device_trigger","tag","notify","infrared"}){V unsupported=V::object();unsupported["type"]=type;assert(describe(unsupported,V()).kind==Kind::None&&!composite(unsupported));}
 puts("PASS: native mappings, command validation, units, invalid measurements, RGB, cover inversion and action semantics");
}
