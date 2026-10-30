"""Trace an actual backup/restore through FS and HTTP into WiFi.begin credentials."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
fixture=(root/'tests/memory-host/no-dynamic.cpp').read_text().split('int main() {',1)[0]
main=r'''
void settle(ArdPortal& portal){for(unsigned i=0;i<1000;++i){fakeMillis+=10;portal.loop();}assert(!portal.portalAndAppConfigBusy());}
void restore(ArdPortal& portal,const String& file){
 WiFiClient client;client.endpoint=std::make_shared<Endpoint>();auto endpoint=client.endpoint;
 String header=String("POST /api/restore HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: ")+String(file.length())+"\r\n\r\n";
 for(size_t i=0;i<header.length();++i)endpoint->input.push_back(uint8_t(header[i]));
 for(size_t i=0;i<file.length();++i)endpoint->input.push_back(uint8_t(file[i]));
 WiFiServer::incoming.push_back(client);
 int before=WiFi.attempts,restarts=ESP.restarts,cleanup=0;bool saved=false;
 portal.onBeforeRestart([&](ArdPortal::RestartReason){++cleanup;});
 portal.onPortalAndAppConfigSaved([&](bool ok,const String&){assert(ok&&portal._rebootPending);saved=true;assert(!portal.setAppConfigValue("stale-shutdown-state",1));});
 for(unsigned i=0;i<20000&&endpoint->connected;++i){++fakeMillis;portal.loop();assert(ESP.restarts==restarts&&WiFi.attempts==before);}
 assert(!endpoint->connected&&saved&&portal._rebootPending&&!portal._pendingReady);
 assert(portal._restartReason==ArdPortal::RestartReason::ConfigurationRestore);
 String response;for(auto byte:endpoint->output)response+=char(byte);
 assert(response.indexOf("202")>=0);
 portal._request="GET /api/status HTTP/1.1\r\n\r\n";portal.handleHttp();
 assert(portal._response.indexOf("\"restarting\":true")>=0);portal.closeHttp();
 for(unsigned i=0;i<1000&&ESP.restarts==restarts;++i){fakeMillis+=10;portal.loop();}
 assert(ESP.restarts==restarts+1&&WiFi.attempts==before&&cleanup==0);
 portal.onBeforeRestart({});portal.onPortalAndAppConfigSaved({});
}
int main(){
 ArdPortal portal;assert(portal.begin());settle(portal);
 auto expected=portal.getPortalConfig();
 expected.ssid="Restore-network";expected.password=u8"Wi-Fi-żółć-S\\\"/-";expected.apPassword="restore-ap-pass";
 assert(portal.setPortalConfig(expected));settle(portal);
 expected=portal.getPortalConfig();
 assert(portal.startConfigBackup());
 String file;for(size_t at=0;at<portal._httpBackupBytes;){String part=portal.configDocumentSlice(portal._request,portal._httpBackupApp,false,portal._httpBackupBytes-portal._request.length()-1,at,256);assert(part.length());file+=part;at+=part.length();}
 portal.closeHttp();
 auto exported=ArdJSON::JSON.parse(file);
 assert(exported["config"]["password"].asString()!=expected.password);
 auto changed=expected;changed.ssid="Other-network";changed.password="other-password";
 assert(portal.setPortalConfig(changed));settle(portal);
 WiFi.state=0;WiFi.stations=1;portal._apActive=true;portal._attempting=false;
 int attempts=WiFi.attempts;
 restore(portal,file);
 assert(WiFi.attempts==attempts);
 assert(ArdPortalJson::equal(portal.getPortalConfig(),expected));
 ArdPortal same;assert(same.begin());settle(same);
 assert(WiFi.staSsid==expected.ssid&&WiFi.staPassword==expected.password);
 // Identical restores also reboot; the AP session cannot suppress the action.
 same._attempting=false;same._apActive=true;same._retrySince=fakeMillis;WiFi.stations=1;WiFi.state=0;
 restore(same,file);
 ArdPortal rebooted;assert(rebooted.begin());settle(rebooted);
 assert(ArdPortalJson::equal(rebooted.getPortalConfig(),expected));
 assert(WiFi.staSsid==expected.ssid&&WiFi.staPassword==expected.password);
 // A failed STA association after restore must also use the decoded AP password.
 WiFi.state=0;WiFi.stations=0;
 fakeMillis=rebooted._wifiSince+rebooted._options.wifiTimeoutMs;
 rebooted.serviceWifi(fakeMillis);
 assert(rebooted._apActive&&!rebooted._attempting);
 assert(WiFi.apPassword==expected.apPassword);
 assert(WiFi.apPassword!=exported["config"]["apPassword"].asString());
 // Failed restore writes never trigger a restart or overwrite the active values.
 auto previous=expected;previous.password="previous-password";
 assert(rebooted.setPortalConfig(previous));settle(rebooted);
 previous=rebooted.getPortalConfig();
 int restarts=ESP.restarts;failFileWrite=true;
 rebooted._http.endpoint=std::make_shared<Endpoint>();rebooted._httpSince=fakeMillis;
 rebooted._request=String("POST /api/restore HTTP/1.1\r\n\r\n")+file;
 assert(rebooted.startConfigRestore(file.length(),rebooted._request.indexOf("\r\n\r\n")));
 assert(!rebooted.setAppConfigValue("during-restore",1));
 for(unsigned i=0;i<3000&&rebooted._httpWaitingStorage;++i){fakeMillis+=10;rebooted.serviceStorage(fakeMillis);}
 assert(rebooted._response.indexOf("500")>=0&&!rebooted._rebootPending&&ESP.restarts==restarts);
 assert(!rebooted._restoreAfterSave&&!rebooted._savePending&&ArdPortalJson::equal(previous,rebooted.getPortalConfig()));
 failFileWrite=false;
 puts("PASS: verified restore -> response drain -> restart, unchanged settings, AP clients, plaintext FS/boot Wi-Fi, protected callbacks and failed writes");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-restore-wifi-') as directory:
 work=Path(directory);(work/'test.cpp').write_text(fixture+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DESP8266','-DARDFS_HOST_TEST',
 '-I'+str(root/'tests/memory-host'),'-I'+str(root/'src'),str(work/'test.cpp'),
 *[str(root/'src'/name) for name in ('ArdStorage.cpp','JsonStream.cpp','JournalReader.cpp','JsonDocumentStream.cpp')],'-o',str(work/'test')],check=True)
 subprocess.run([str(work/'test')],check=True)
