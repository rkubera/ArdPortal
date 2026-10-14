# Exercise actual AP startup on dual-band and single-band Wi-Fi adapters.
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parent.parent
source=(root/'src/ArdPortalImpl.h').read_text();start=source.index('void ArdPortal::startAP()');end=source.index('\n/**',start);method=source[start:end]
cpp='''#define ARDPORTAL_ENABLE_WIFI 1
#define ARDPORTAL_ENABLE_WWW 1
#include <Arduino.h>
#include <cassert>
uint32_t fakeMillis=0;
constexpr int WIFI_AP_STA=1;
struct IP {};
struct Wifi {int channelValue=44,lastChannel=0,calls=0;void mode(int){}int channel(){return channelValue;}bool softAP(const char*,const char*,int channel=1){lastChannel=channel;++calls;return true;}IP softAPIP(){return {};}} WiFi;
namespace DNSReplyCode {constexpr int NoError=0;}
namespace ArdUILanguage {enum class Key{s_101};String text(Key){return "AP";}}
class ArdPortal {public:struct Config{uint8_t apBand=24;String ssid,apName="Test",apPassword;}_config;enum class WifiState{FallbackAP,NoCredentials};struct DNS{void setErrorReplyCode(int){}void start(int,const char*,IP){}}_dns;bool _apActive=false,connected=false;enum class ApPolicy{Automatic,AlwaysOn,Disabled};ApPolicy _apPolicy=ApPolicy::Automatic;WifiState _wifiState;uint32_t _retrySince=0;bool wifiConnected(){return connected;}void log(const String&){}void startAP();};
'''+method+'''
int main(){ArdPortal p;p.startAP();assert(WiFi.lastChannel==1);p._apActive=false;p._config.apBand=5;p.startAP();
#if ARDPORTAL_SUPPORT_DUAL_BAND_AP
assert(WiFi.lastChannel==36);p._apActive=false;p.connected=true;p._config.apBand=24;p.startAP();assert(WiFi.lastChannel==44);
#else
assert(WiFi.lastChannel==1);
#endif
int before=WiFi.calls;p.startAP();assert(WiFi.calls==before);}
'''
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)/'test.cpp';path.write_text(cpp)
 for capable in [0,1]:
  exe=Path(directory)/('test'+str(capable));subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-DARDPORTAL_SUPPORT_DUAL_BAND_AP='+str(capable),'-I'+str(root/'tests/host'),str(path),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
print('PASS: 2.4/5 GHz AP startup, shared STA channel and single-band behavior')
