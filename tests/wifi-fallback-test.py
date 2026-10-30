"""Exercise actual STA timeout/AP fallback, including rejected and transient AP starts."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
fixture=(root/'tests/memory-host/no-dynamic.cpp').read_text().split('int main() {',1)[0]
main=r'''
void configure(ArdPortal& portal){
 WiFi=WiFiStub();fakeMillis=0;
 portal._config.ssid="Home";portal._config.password="incorrect-wifi-password";
 portal._config.apName="Fallback-test";portal._config.apPassword="correct-ap-password";
 portal.connectWifi();
 assert(portal._attempting&&WiFi.attempts==1&&WiFi.apStarts==0);
}
int main(){
 ArdPortal badWifi;configure(badWifi);
 fakeMillis=badWifi._options.wifiTimeoutMs-1;badWifi.serviceWifi(fakeMillis);
 assert(!badWifi._apActive&&WiFi.apStarts==0);
 ++fakeMillis;badWifi.serviceWifi(fakeMillis);
 assert(badWifi._apActive&&!badWifi._attempting&&badWifi._wifiResult==2);
 assert(badWifi._wifiState==ArdPortal::WifiState::FallbackAP);
 assert(WiFi.apPassword=="correct-ap-password"&&WiFi.apStarts==1);
 WiFi.stations=1;fakeMillis+=badWifi._options.retryMs;badWifi.serviceWifi(fakeMillis);
 assert(WiFi.attempts==1&&badWifi._apActive); // An AP client pauses STA retries.
 WiFi.stations=0;badWifi.serviceWifi(fakeMillis);
 assert(WiFi.attempts==2&&badWifi._apActive&&WiFi.apStarts==1);
 fakeMillis+=badWifi._options.wifiTimeoutMs;badWifi.serviceWifi(fakeMillis);
 assert(badWifi._apActive&&WiFi.apStarts==1); // Existing AP is not restarted on wrong STA password.

 ArdPortal failure;configure(failure);WiFi.apStartResult=false;
 fakeMillis=failure._options.wifiTimeoutMs;failure.serviceWifi(fakeMillis);
 assert(!failure._apActive&&WiFi.apStarts==1);
 assert(failure._wifiState==ArdPortal::WifiState::Disconnected);
 int attempts=WiFi.attempts;
 fakeMillis+=failure._options.retryMs-1;failure.serviceWifi(fakeMillis);
 assert(WiFi.apStarts==1&&WiFi.attempts==attempts);
 WiFi.apStartResult=true;++fakeMillis;failure.serviceWifi(fakeMillis);
 assert(WiFi.apStarts==2&&failure._apActive&&WiFi.attempts==attempts);
 assert(failure._wifiState==ArdPortal::WifiState::FallbackAP);
 WiFi.stations=1;fakeMillis+=failure._options.retryMs;failure.serviceWifi(fakeMillis);
 assert(WiFi.attempts==attempts); // A recovered AP gets a stable association window.
 WiFi.stations=0;failure.serviceWifi(fakeMillis);
 assert(WiFi.attempts==attempts+1);
 assert(WiFi.apPassword=="correct-ap-password"); // Driver recovery never rewrites credentials.

 ArdPortal persistent;configure(persistent);WiFi.apStartResult=false;
 fakeMillis=persistent._options.wifiTimeoutMs;persistent.serviceWifi(fakeMillis);
 fakeMillis+=persistent._options.retryMs;persistent.serviceWifi(fakeMillis);
 assert(!persistent._apActive&&persistent._wifiState==ArdPortal::WifiState::Connecting);
 assert(WiFi.apStarts==2&&WiFi.attempts==2);

 ArdPortal disabled;configure(disabled);disabled._apPolicy=ArdPortal::ApPolicy::Disabled;
 fakeMillis=disabled._options.wifiTimeoutMs;disabled.serviceWifi(fakeMillis);
 fakeMillis+=disabled._options.retryMs;disabled.serviceWifi(fakeMillis);
 assert(WiFi.apStarts==0&&WiFi.attempts==2&&!disabled._apActive);

 ArdPortal paused;configure(paused);WiFi.apStartResult=false;
 fakeMillis=paused._options.wifiTimeoutMs;paused.serviceWifi(fakeMillis);
 paused._wifiAutoReconnect=false;WiFi.apStartResult=true;
 fakeMillis+=paused._options.retryMs;paused.serviceWifi(fakeMillis);
 assert(paused._apActive&&WiFi.attempts==1&&WiFi.apStarts==2);
 ArdPortal connected;configure(connected);WiFi.state=WL_CONNECTED;WiFi.apStartResult=false;
 connected._wifiState=ArdPortal::WifiState::Connected;connected.startAP();
 assert(!connected._apActive&&connected._wifiState==ArdPortal::WifiState::Connected);

 puts("PASS: wrong Wi-Fi password starts AP at timeout; client protection; honest AP failure state, bounded retry/recovery, disabled AP and paused STA");
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-wifi-fallback-') as directory:
 work=Path(directory);(work/'test.cpp').write_text(fixture+main)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
 '-DESP8266','-DARDFS_HOST_TEST','-I'+str(root/'tests/memory-host'),'-I'+str(root/'src'),str(work/'test.cpp'),
 *[str(root/'src'/name) for name in ('ArdStorage.cpp','JsonStream.cpp','JournalReader.cpp','JsonDocumentStream.cpp')],'-o',str(work/'test')],check=True)
 subprocess.run([str(work/'test')],check=True)
