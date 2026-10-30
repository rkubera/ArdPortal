"""Round-trip the actual prefix-free config codec, including malformed input and limits."""
from pathlib import Path
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = '#define ARDPORTAL_ENABLE_DYNAMIC_PAGES 0\n#define ARDPORTAL_ENABLE_OTA 0\n' + (root / 'tests/memory-host/mqtt-state-topic.cpp').read_text().split('int main() {', 1)[0].replace('../../src/', str(root / 'src') + '/')
known = json.dumps(bytes(b ^ 0x0f if b < 0xc0 else b for b in 'Zażółć 🔒'.encode()).decode(), ensure_ascii=False)
main = r'''
int main() {
  // Every Unicode scalar remains valid UTF-8 and is changed reversibly.
  for(uint32_t cp=0;cp<=0x10ffff;++cp) {
    if(cp>=0xd800 && cp<=0xdfff) continue;
    char bytes[4];unsigned n;
    if(cp<0x80){bytes[0]=char(cp);n=1;}
    else if(cp<0x800){bytes[0]=char(0xc0|(cp>>6));bytes[1]=char(0x80|(cp&63));n=2;}
    else if(cp<0x10000){bytes[0]=char(0xe0|(cp>>12));bytes[1]=char(0x80|((cp>>6)&63));bytes[2]=char(0x80|(cp&63));n=3;}
    else{bytes[0]=char(0xf0|(cp>>18));bytes[1]=char(0x80|((cp>>12)&63));bytes[2]=char(0x80|((cp>>6)&63));bytes[3]=char(0x80|(cp&63));n=4;}
    String raw,encoded,decoded;
    assert(raw.concat(bytes,n));
    assert(ArdPortalJson::mask(raw,encoded));
    assert(encoded.length()==n && encoded!=raw && ArdJSON::validUtf8(encoded));
    assert(ArdPortalJson::mask(encoded,decoded)&&decoded==raw);
  }
  String known=u8"Zażółć 🔒";
  assert(ArdPortalJson::mask(known,known)&&known==KNOWN);
  ArdPortal::PortalConfig c,decoded;
  c.deviceName="Mask test";
  c.ssid=u8"Sieć domowa";
  c.password=u8"Wi-Fi żółć 🔒";
  c.apPassword=u8"AP-zażółć";
  c.caCert="-----BEGIN CERTIFICATE-----\nTEST\n-----END CERTIFICATE-----\n";
  auto stored=ArdPortalJson::value(c);
  assert(stored["password"].asString()==c.password);
  assert(stored["apPassword"].asString()==c.apPassword);
  assert(stored["caCert"].asString()==c.caCert);
  assert(ArdPortalJson::decode(ArdPortalJson::encode(c),decoded)&&ArdPortalJson::equal(c,decoded));
  auto document=ArdPortalJson::value(c,true);
  assert(document["version"].asDouble()==1);
  assert(document["ssid"].asString()==c.ssid);
  for(const char* key:{"password","apPassword","caCert"}) {
    String encoded=document[key].asString();
    assert(encoded.length());
  }
  assert(document["password"].asString()!=c.password);
  assert(document["apPassword"].asString()!=c.apPassword);
  assert(document["caCert"].asString()!=c.caCert);
  String json=ArdPortalJson::encode(c,ArdJsonCodec(),true);
  assert(json.length()&&ArdPortalJson::decode(json,decoded,ArdJsonCodec(),true)&&ArdPortalJson::equal(c,decoded));
  auto invalidType=document;
  invalidType["password"]=17;
  assert(!ArdPortalJson::decodeValue(invalidType,decoded,true)&&ArdPortalJson::equal(c,decoded));
  for(uint8_t invalidByte:{uint8_t(0),uint8_t(255)}) {
    String invalid;
    char b=char(invalidByte);assert(invalid.concat(&b,1));
    assert(ArdPortalJson::mask(invalid,invalid));
    document["password"]=invalid;
    assert(!ArdPortalJson::decodeValue(document,decoded,true));
  }
  c.password=std::string(64,'w');
  c.apPassword=std::string(63,'a');
  std::string certificate="-----BEGIN CERTIFICATE-----\n",end="\n-----END CERTIFICATE-----\n";
  certificate.append(4096-certificate.size()-end.size(),'C');
  certificate+=end;
  c.caCert=certificate;
  c.host="mqtt.local";c.mqttTls=true;
  auto maximum=ArdPortalJson::value(c,true);
  assert(maximum["caCert"].asString().length()==4096);
  json=ArdPortalJson::encode(c,ArdJsonCodec(),true);
  assert(json.length()&&ArdPortalJson::decode(json,decoded,ArdJsonCodec(),true)&&ArdPortalJson::equal(c,decoded));
  String tooLong=std::string(4097,'C');
  assert(ArdPortalJson::mask(tooLong,tooLong));
  maximum["caCert"]=tooLong;
  assert(!ArdPortalJson::decodeValue(maximum,decoded,true));
  c.password="";c.apPassword="";c.caCert="";c.mqttTls=false;
  json=ArdPortalJson::encode(c,ArdJsonCodec(),true);
  assert(json.length()&&ArdPortalJson::decode(json,decoded,ArdJsonCodec(),true)&&ArdPortalJson::equal(c,decoded));
  String input="unchanged";
  String::reserveFailCountdownForTest()=0;
  assert(!ArdPortalJson::mask(input,input)&&input=="unchanged");
  String::reserveFailCountdownForTest()=SIZE_MAX;
  // A full certificate plus near-limit AppConfig keeps the original byte budgets.
  ArdPortal portal;
  assert(portal.begin());
  for(unsigned i=0;i<1000;++i){fakeMillis+=10;portal.loop();}
  c.password=std::string(64,'w');c.apPassword=std::string(63,'a');c.caCert=certificate;
  assert(portal.setPortalConfig(c));
  assert(portal.setAppConfigValue("bulk",String(std::string(8000,'x'))));
  for(unsigned i=0;i<1000;++i){fakeMillis+=10;portal.loop();}
  assert(!portal.portalAndAppConfigBusy());
  assert(ArdPortalJson::encode(c).length()+8011+18<=ArdPortal::MaxConfigDocumentBytes);
  ArdPortal rebooted;
  assert(rebooted.begin());
  for(unsigned i=0;i<1000;++i){fakeMillis+=10;rebooted.loop();}
  assert(portal.getPortalConfig().caCert==c.caCert);
  assert(portal.getPortalConfig().password==c.password);
  assert(portal.getPortalConfig().apPassword==c.apPassword);
  assert(ArdPortalJson::equal(portal.getPortalConfig(),rebooted.getPortalConfig()));
  assert(rebooted.getAppConfigValue("bulk").asString().length()==8000);
  // Escaping may enlarge a masked backup; the FS retains its original limits.
  auto special=rebooted.getPortalConfig();
  special.caCert=std::string(4096,'S');special.mqttTls=false;
  assert(ArdPortal::validPortalConfig(special));
  auto exportRoot=ArdJSON::JSONVar::object();
  exportRoot["config"]=ArdPortalJson::value(special,true);
  exportRoot["app"]=ArdJSON::JSONVar::object();
  exportRoot["app"]["bulk"]=String(std::string(8000,'x'));
  String backupFile=ArdJSON::JSON.stringify(exportRoot);
  assert(backupFile.length()>ArdPortal::MaxConfigDocumentBytes);
  assert(backupFile.length()<=ArdPortal::MaxBackupDocumentBytes);
  rebooted._http.endpoint=std::make_shared<Endpoint>();
  rebooted._httpSince=fakeMillis;
  rebooted._request="POST /api/restore HTTP/1.1\r\n\r\n";
  assert(rebooted.startConfigRestore(backupFile.length(),rebooted._request.indexOf("\r\n\r\n")));
  for(size_t at=0;at<backupFile.length();){
    for(unsigned n=0;n<256&&at<backupFile.length();++n,++at) rebooted._http.endpoint->input.push_back(uint8_t(backupFile[at]));
    rebooted.serviceHttpIo(fakeMillis);
  }
  assert(rebooted._httpWaitingStorage&&rebooted._savePending);
  for(unsigned i=0;i<1000;++i){fakeMillis+=10;rebooted.serviceStorage(fakeMillis);}
  assert(rebooted._response.indexOf("202")>=0);
  const int restarts=ESP.restarts;
  rebooted.closeHttp();
  for(unsigned i=0;i<1000&&ESP.restarts==restarts;++i){fakeMillis+=10;rebooted.loop();}
  assert(ESP.restarts==restarts+1&&rebooted.getPortalConfig().caCert==special.caCert);
  puts("PASS: prefix-free portable XOR, every Unicode scalar, typed input, empty/max fields and OOM");
}
'''.replace('KNOWN', known)
with tempfile.TemporaryDirectory(prefix='ardportal-config-mask-') as directory:
    work = Path(directory)
    (work / 'test.cpp').write_text(fixture + main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-DESP8266','-DARDFS_HOST_TEST','-I'+str(root/'tests/memory-host'),'-I'+str(root/'src'),
                    str(work/'test.cpp'),*[str(root/'src'/name) for name in
                    ('ArdStorage.cpp','JsonStream.cpp','JournalReader.cpp','JsonDocumentStream.cpp')],
                    '-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
