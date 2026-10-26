from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
fixture=r'''
#include <Arduino.h>
#include <cassert>
#include <functional>
#define ARDPORTAL_ENABLE_MQTT 0
#define ARDPORTAL_ENABLE_WIFI 1
#define ARDPORTAL_ENABLE_WWW 1
#define ARDPORTAL_SUPPORT_WEBSOCKET 1
#define ARDPORTAL_ENABLE_ZIGBEE 1
#define ARDPORTAL_ENABLE_CONSOLE 1
#define ARDPORTAL_ENABLE_CONSOLE_MESSAGES 1
#define WIFI_STA 1
#define WIFI_OFF 0
uint32_t fakeMillis=0;
struct Wifi {bool stop=true;unsigned disconnects=0;bool enableAP(bool enable){assert(!enable);return stop;}bool mode(int){return stop;}void disconnect(bool,bool){++disconnects;}} WiFi;
struct Dns {unsigned stops=0;void stop(){++stops;}};
struct Server {unsigned begins=0,ends=0;void begin(){++begins;}void stop(){++ends;}};
class ArdPortal {public:
 enum class WifiState{NoCredentials,Connecting,Connected,FallbackAP,Disconnected};enum class ApPolicy{Automatic,AlwaysOn,Disabled};
 struct Matter {bool started(){return true;}} _matter; int _matterWs=0;
 bool _portalSuspended=false,_resumeWifiAutoReconnect=true; unsigned _retrySince=0;
 bool _configurationReady=true,busy=false,ota=false,reboot=false,trial=false,connected=false;
 bool _rebootPending=false,_scanning=false,_scanRequested=false,_attempting=false,_wasConnected=false,_apActive=false,_wifiAutoReconnect=true,_scanSucceeded=false,_apiApActive=false;
 unsigned _wifiRevision=0,_wifiResult=0,_scanId=0,_scanCompletedId=0,_apiScanCompletedId=0,connects=0,closes=0;
 struct {String ssid="test";} _config;Dns _dns;String _networks="[]";WifiState _wifiState=WifiState::NoCredentials,_apiWifiState=WifiState::NoCredentials;ApPolicy _apPolicy=ApPolicy::Automatic;
 std::function<void(WifiState)> _wifiStateCallback;std::function<void(bool,const String&)> _wifiScanCallback;std::function<void(bool)> _apStateCallback;
 bool portalAndAppConfigBusy(){return busy;}bool otaActive(){return ota;}bool mqttTrialActive(){return trial;}bool wifiConnected(){return connected;}bool wifiScanBusy(){return _scanning||_scanRequested;}
 void connectWifi(){++connects;_attempting=true;_wifiState=WifiState::Connecting;}void closeMqtt(){++closes;}void startAP(){_apActive=true;}
 Server _server; unsigned httpCloses=0,wsCloses=0; int _eventWs=0,_zigbeeWs=0,_consoleWs=0;
 void closeHttp(){++httpCloses;}void closeWebSocket(int&){++wsCloses;}
 bool suspendPortal();bool resumePortal();bool connectWifiStation();bool disconnectWifiStation();bool scanWifiNetworks();bool setApPolicy(ApPolicy);void serviceNetworkApi();
};
'''
main=r'''
int main(){
 ArdPortal p;
 p.busy=true;assert(!p.suspendPortal());p.busy=false;
 p._scanRequested=true;assert(!p.suspendPortal());p._scanRequested=false;
 WiFi.stop=false;assert(!p.suspendPortal()&&!p._portalSuspended);WiFi.stop=true;
 assert(p.suspendPortal()&&p._portalSuspended&&!p._wifiAutoReconnect);
 assert(p.httpCloses==1&&p.wsCloses==3&&p._server.ends==1&&p._dns.stops==1);
 assert(p.suspendPortal()&&!p.connectWifiStation()&&!p.scanWifiNetworks()&&!p.setApPolicy(ArdPortal::ApPolicy::AlwaysOn));
 WiFi.stop=false;assert(!p.resumePortal()&&p._portalSuspended);WiFi.stop=true;
 assert(p.resumePortal()&&!p._portalSuspended&&p._wifiAutoReconnect);
 assert(p._server.begins==1);
 p._wifiAutoReconnect=false;p._apPolicy=ArdPortal::ApPolicy::AlwaysOn;
 assert(p.suspendPortal()&&p.resumePortal()&&!p._wifiAutoReconnect&&p._apActive);
 p._apActive=false;p._apPolicy=ArdPortal::ApPolicy::Automatic;
 p.connects=p.closes=0;
 p.busy=true;assert(!p.connectWifiStation()&&!p.disconnectWifiStation()&&!p.setApPolicy(ArdPortal::ApPolicy::AlwaysOn));p.busy=false;
 assert(p.disconnectWifiStation()&&!p._wifiAutoReconnect&&WiFi.disconnects==1);
 assert(p.connectWifiStation()&&p._wifiAutoReconnect&&p.connects==1);
 unsigned states=0,scans=0,aps=0;p._wifiStateCallback=[&](auto){++states;};p._apStateCallback=[&](bool){++aps;};p._wifiScanCallback=[&](bool ok,const String& json){assert(ok&&json=="[]");++scans;};
 p.serviceNetworkApi();p.serviceNetworkApi();assert(states==1);
 assert(p.scanWifiNetworks()&&!p.scanWifiNetworks());p._scanRequested=false;p._scanSucceeded=true;p._scanCompletedId=p._scanId;p.serviceNetworkApi();p.serviceNetworkApi();assert(scans==1);
 assert(p.setApPolicy(ArdPortal::ApPolicy::AlwaysOn)&&p._apActive);p.serviceNetworkApi();assert(aps==1);
 WiFi.stop=false;assert(!p.setApPolicy(ArdPortal::ApPolicy::Disabled)&&p._apActive&&p._apPolicy==ArdPortal::ApPolicy::AlwaysOn);WiFi.stop=true;
 assert(p.setApPolicy(ArdPortal::ApPolicy::Disabled)&&!p._apActive);p.serviceNetworkApi();assert(aps==2);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+(root/'src/NetworkApiImpl.h').read_text().replace('#pragma once','')+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'tests/host'),str(p),'-o',str(Path(d)/'test')],check=True)
 subprocess.run([str(Path(d)/'test')],check=True)
 # Matter Wi-Fi must retain its radio even before application commissioning.
 p.write_text('#define ARDPORTAL_ENABLE_MATTER 1\n#define ARDPORTAL_MATTER_THREAD 0\n'+fixture+(root/'src/NetworkApiImpl.h').read_text().replace('#pragma once','')+'\nint main(){ArdPortal p;assert(!p.suspendPortal());assert(!p._portalSuspended);assert(p.closes==0&&p.httpCloses==0);}')
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(root/'tests/host'),str(p),'-o',str(Path(d)/'matter')],check=True)
 subprocess.run([str(Path(d)/'matter')],check=True)
print('PASS: network API guards, reconnect policy, scan/state notifications and failed AP stop preservation')
