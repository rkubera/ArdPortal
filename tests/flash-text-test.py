from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'src/ArdPortalImpl.h').read_text()
start=source.index('void ArdPortal::replyMessage(')
method=source[start:source.index('\n/**',start)]
fixture=r'''
#define ARDUI_DEFINE_STORAGE_MESSAGES
#include "Language.h"
#include <cassert>
uint32_t fakeMillis=0;
class ArdPortal {public:
 String _response;size_t length=0;int status=0;
 void responseHeader(int code,const char*,size_t bytes){status=code;length=bytes;_response=String();}
 void reply(int code,const char* type,const String& body){responseHeader(code,type,body.length());_response+=body;}
 void replyMessage(int,ArdUILanguageData::Key);
};
int main(){
 String english="en",unknown="missing";
 const size_t before=String::cStringConstructionsForTest();
 assert(ArdUILanguage::find(english)==0&&ArdUILanguage::find(unknown)==-1);
 assert(ArdUILanguage::messageId(F("s_194"))==194);
 for(auto key:{F(""),F("s"),F("s_"),F("s_0"),F("s_01"),F("s_65536"),F("s_12x")})assert(!ArdUILanguage::messageId(key));
 assert(String::cStringConstructionsForTest()==before);
 const char* pointer=ArdUILanguage::textFlash(ArdUILanguage::Key::s_194);
 assert(pointer&&strlen_P(pointer));
 assert(String::cStringConstructionsForTest()==before);
 ArdPortal p;p.replyMessage(503,ArdUILanguage::Key::s_194);
 assert(p.status==503&&p.length==strlen_P(pointer));
 assert(ardFlashEqual(p._response,pointer));
 assert(String::cStringConstructionsForTest()==before);
 const auto copies=String::cStringConstructionsForTest();
 String translated=ArdUILanguage::text(F("s_194"));
 assert(String::cStringConstructionsForTest()==copies+1);
 assert(translated==ArdUILanguage::text("s_194"));
 assert(ArdUILanguage::storageText(F("s_194"))==ArdUILanguage::storageText("s_194"));
 assert(ArdUILanguage::text(F("missing"))=="missing");
 p.replyMessage(400,static_cast<ArdUILanguage::Key>(65535));assert(p._response=="s_65535");
 String embedded;embedded+='e';embedded+='n';embedded+=char(0);embedded+='x';
 assert(!ardFlashEqual(embedded,"en"));
}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory)/'test.cpp'
    # Member definitions precede main, matching the actual library inclusion order.
    split=fixture.index('int main()')
    path.write_text(fixture[:split]+method+'\n'+fixture[split:])
    executable=Path(directory)/'test'
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(root/'tests/host'),'-I'+str(root/'src'),str(path),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
print('PASS: flash lookup/comparison/key decoding and HTTP translation without temporary Strings; fallback preserved')
