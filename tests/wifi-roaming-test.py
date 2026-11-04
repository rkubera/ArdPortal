#!/usr/bin/env python3
"""Exercise actual station setup/recovery for partial and absent SDK features."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
impl = (root/'src/ArdPortalImpl.h').read_text()
methods = ''
for name in ('connectWifi', 'serviceWifi'):
    start = impl.index('void ArdPortal::'+name+'(')
    methods += impl[start:impl.index('\n/**', start)]+'\n'
fixture = r'''
#include "Arduino.h"
#include <cassert>
#include <cstring>
uint32_t fakeMillis=0;
int ntpStarts=0;
String lastNtpServer1,lastNtpServer2;
enum {WIFI_IF_STA=0,ESP_OK=0,WIFI_STA=1};
struct wifi_config_t {struct {
 unsigned char ssid[32]={},password[64]={};
 int security=7,channel=0;bool bssid_set=false;
#if CONFIG_ESP_WIFI_11KV_SUPPORT
 int rm_enabled=0,btm_enabled=0;
#if CONFIG_ESP_WIFI_MBO_SUPPORT
 int mbo_enabled=0;
#endif
#endif
#if CONFIG_ESP_WIFI_11R_SUPPORT
 int ft_enabled=0;
#endif
}sta;};
wifi_config_t current;
bool getFail=false,setFail=false,prepareFail=false;
int getCalls=0,setCalls=0;
int esp_wifi_get_config(int,wifi_config_t* c){++getCalls;*c=current;return getFail?-1:0;}
int esp_wifi_set_config(int,const wifi_config_t* c){++setCalls;if(setFail)return -1;current=*c;return 0;}
struct Wifi {
 int starts=0,prepares=0;bool connected=false;
 void begin(const char* ssid,const char* password,int=0,const void* =nullptr,bool connect=true){
  if(connect)++starts;else ++prepares;
  if(prepareFail&&!connect)return;
  current={};memcpy(current.sta.ssid,ssid,strlen(ssid));memcpy(current.sta.password,password,strlen(password));
 }
 void begin(){++starts;}
 unsigned softAPgetStationNum(){return 0;}
 void disconnect(bool,bool){connected=false;}
 bool enableAP(bool){return true;}
 void mode(int){}
}WiFi;
struct ArdPortal {
 enum class WifiState {Connecting,Connected,Disconnected,FallbackAP,NoCredentials};
 enum class ApPolicy {Automatic,AlwaysOn,Disabled};
 struct Config {String ssid="mesh",password="secret";} _config;
 struct Options {uint32_t wifiTimeoutMs=10000,retryMs=10000;} _options;
 String _ntpServer1="pool.ntp.org",_ntpServer2;
 bool _portalSuspended=false,_wifiAutoReconnect=true,_attempting=false,_wasConnected=false;
 bool _apActive=false,_scanning=false,_http=false,_wifiRoamPending=false;
 uint32_t _wifiSince=0,_retrySince=0,_wifiRoamSince=0;
 unsigned _wifiResult=0,closed=0;
 WifiState _wifiState=WifiState::NoCredentials;
 ApPolicy _apPolicy=ApPolicy::Automatic;
 bool wifiConnected(){return WiFi.connected;}
 void closeMqtt(){++closed;}
 void startAP(){_apActive=true;}
 void connectWifi();void serviceWifi(uint32_t);
};
'''
main = r'''
int main(){
 ArdPortal p;
 p.connectWifi();
#if !ARDPORTAL_ENABLE_WIFI
 assert(WiFi.starts==0 && WiFi.prepares==0);return 0;
#else
 assert(WiFi.starts==1 && current.sta.security==7);
 assert(!memcmp(current.sta.ssid,"mesh",4));
 assert(!memcmp(current.sta.password,"secret",6));
#if defined(ESP32) && (CONFIG_ESP_WIFI_11KV_SUPPORT || CONFIG_ESP_WIFI_11R_SUPPORT)
 assert(WiFi.prepares==1 && setCalls==1);
 assert(!current.sta.bssid_set && current.sta.channel==0);
#if CONFIG_ESP_WIFI_11KV_SUPPORT
 assert(current.sta.rm_enabled && current.sta.btm_enabled);
#if CONFIG_ESP_WIFI_MBO_SUPPORT
 assert(current.sta.mbo_enabled);
#endif
#endif
#if CONFIG_ESP_WIFI_11R_SUPPORT
 assert(current.sta.ft_enabled);
#endif
 // Failure paths must still associate with the requested credentials.
 getFail=true;p.connectWifi();assert(WiFi.starts==2);getFail=false;
 setFail=true;p.connectWifi();assert(WiFi.starts==3);setFail=false;
 prepareFail=true;current.sta.ssid[0]='X';p.connectWifi();assert(WiFi.starts==4);prepareFail=false;
 p._config.ssid=String("12345678901234567890123456789012");
 p._config.password=String("1234567890123456789012345678901234567890123456789012345678901234");
 p.connectWifi();assert(WiFi.starts==5); // Fixed-size SDK strings need no trailing NUL.
 WiFi.connected=true;p.serviceWifi(100);assert(p._wasConnected);
 int starts=WiFi.starts;
 WiFi.connected=false;p.serviceWifi(200);p.serviceWifi(2199);
 assert(WiFi.starts==starts && p.closed==0);
 WiFi.connected=true;p.serviceWifi(2200);assert(!p._wifiRoamPending && p.closed==0);
 // A second loss receives a fresh, bounded grace period, including millis wrap.
 WiFi.connected=false;p.serviceWifi(0xffffff00U);p.serviceWifi(0xffffff00U+1999U);
 assert(WiFi.starts==starts);
 p.serviceWifi(0xffffff00U+2000U);assert(WiFi.starts==starts+1 && p.closed==1);
#else
 assert(WiFi.prepares==0 && getCalls==0 && setCalls==0);
 WiFi.connected=true;p.serviceWifi(100);WiFi.connected=false;p.serviceWifi(200);
 assert(WiFi.starts==2 && p.closed==1); // Unsupported SDK retains immediate recovery.
#endif
#endif
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-roaming-') as directory:
    work = Path(directory)
    source = work/'test.cpp'
    source.write_text(fixture+methods+main)
    for name, flags in (
        ('esp8266', ['-DESP8266=1']),
        ('esp32-disabled', ['-DESP32=1']),
        ('kv', ['-DESP32=1','-DCONFIG_ESP_WIFI_11KV_SUPPORT=1']),
        ('r', ['-DESP32=1','-DCONFIG_ESP_WIFI_11R_SUPPORT=1']),
        ('kvr-mbo', ['-DESP32=1','-DCONFIG_ESP_WIFI_11KV_SUPPORT=1','-DCONFIG_ESP_WIFI_11R_SUPPORT=1','-DCONFIG_ESP_WIFI_MBO_SUPPORT=1']),
        ('network-free', ['-DESP32=1','-DARDPORTAL_ENABLE_WIFI=0']),
    ):
        subprocess.run([os.environ.get('CXX','c++'), '-std=c++17', '-Wall','-Wextra','-Werror',
                        '-fsanitize=address,undefined','-I'+str(root/'tests/host'),
                        '-DARDPORTAL_ENABLE_WIFI='+('0' if name=='network-free' else '1'),
                        *flags,str(source),'-o',str(work/name)],check=True)
        subprocess.run([str(work/name)],check=True)
print('PASS: SDK k/v/r combinations, credentials, fallback, roaming grace, rollover and ordinary recovery')
