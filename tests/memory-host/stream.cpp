#include "Arduino.h"
#include "ArdFSHostVolume.h"
#include "../../src/ArdFS.h"
#include <cassert>
uint32_t fakeMillis=0;SerialStub Serial;EspStub ESP;
bool failMount=false,failFormat=false,failFileWrite=false,failRename=false;
std::map<std::string,std::vector<uint8_t>> fakeFiles;
void drain(ArdFS& fs){for(int i=0;i<10000&&fs.busy();++i)fs.loop();assert(!fs.busy());}
String read(){ArdFS fs;fs.begin();drain(fs);String result;assert(fs.read("/test.json",[&](const ArdFS::Result& r){assert(r.ok&&r.found);result=r.data;}));drain(fs);return result;}
void scannerTests(){
 String decoded="😀z\n";uint32_t gen=UINT32_MAX,crc=UINT32_MAX;
 for(unsigned i=0;i<4;++i)crc=ArdJournalStream::update(crc,uint8_t(gen>>(8*i)));
 for(size_t i=0;i<decoded.length();++i)crc=ArdJournalStream::update(crc,uint8_t(decoded[i]));
 String record=String("{\"journal\":1,\"generation\":")+String(gen)+",\"crc32\":"+String(~crc)+",\"data\":\"\\ud83d\\ude00\\u007a\\n\" } \n";
 ArdJournalStream scan;String output;auto emit=[&](uint8_t b,size_t){output+=char(b);return true;};
 for(size_t i=0;i<record.length();++i)scan.input(record[i],emit);
 assert(scan.valid()&&output==decoded&&scan.generation==gen);
 record.replace("\\ude00","\\ud800");scan=ArdJournalStream();output=String();for(size_t i=0;i<record.length();++i)scan.input(record[i],emit);assert(!scan.valid());
}
int main(){scannerTests();using namespace ArdJSON;JSONVar data=JSONVar::object();for(int i=0;i<300;++i)data[String("field")+String(i)]=i;data["text"]="zażółć 😀 \\ \" \n";Limits l;l.maxNodes=4096;String json=JSON.stringify(data,false,nullptr,l);assert(json.length()>4000);
ArdFS fs;fs.begin();drain(fs);
auto save=[&](bool changed){bool done=false;assert(fs.writeJson("/test.json",json.length(),[&](size_t at,size_t n){assert(n<=256);return json.substring(at,at+n);},[&](const ArdFS::Result&r){assert(r.ok&&r.changed==changed);done=true;}));drain(fs);assert(done);};
String::reserveLimitForTest()=512;save(true);save(false);String::reserveLimitForTest()=SIZE_MAX;assert(read()==json);
String old=json;data["field3"]=777;json=JSON.stringify(data,false,nullptr,l);String::reserveLimitForTest()=512;save(true);String::reserveLimitForTest()=SIZE_MAX;assert(read()==json);assert(fakeMaxTransfer<=256);
// Corrupt the newest generation; the older valid slot remains readable.
auto &slot=fakeFiles["/test.json.journal"];assert(slot.size());slot[slot.size()-5]^=1;assert(read()==old);
String::reserveLimitForTest()=512;save(true);String::reserveLimitForTest()=SIZE_MAX;assert(read()==json);
// Producer allocation failure and device write failure cannot replace the good record.
bool failed=false;assert(fs.writeJson("/test.json",json.length(),[](size_t,size_t){return String();},[&](const ArdFS::Result&r){failed=!r.ok;}));drain(fs);assert(failed&&read()==json);
json="{\"new\":true}";failFileWrite=true;failed=false;assert(fs.writeJson("/test.json",json.length(),[&](size_t at,size_t n){return json.substring(at,at+n);},[&](const ArdFS::Result&r){failed=!r.ok;}));drain(fs);failFileWrite=false;assert(failed&&read()!=json);
}
