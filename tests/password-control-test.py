#!/usr/bin/env python3
"""Trace local password registration, HTTP redaction, MQTT exclusion and backup/restore."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = (root/'tests/memory-host/dynamic-patch.cpp').read_text().split('int main() {',1)[0]
fixture = fixture.replace('"../../src/', '"'+str(root/'src')+'/')
main = r'''
#include "MatterFields.h"
#include "ZigbeeFields.h"
using V=ArdJSON::JSONVar;
void registerPage(ArdPortal& p){
 assert(p.startAppConfigPageRegistration(String(R"({"id":"secrets","name":"Secrets","fields":[{"id":"token","type":"password","name":"Token","default":"firmware-secret","maxLength":8192},{"id":"label","type":"edit","name":"Label"}]})")));
 for(unsigned i=0;i<1000 && p.appConfigPageRegistrationBusy();++i)p.serviceAppConfigPageRegistration();
 if(p._dynamic.count()!=2){auto e=p.appConfigRegistrationError();fprintf(stderr,"%s: %s\n",e.stage,e.reason.c_str());}
 assert(p._dynamic.count()==2);
}
String backup(ArdPortal& p){
 assert(p.startConfigBackup());String out;
 for(size_t at=0;at<p._httpBackupBytes;){String part=p.configDocumentSlice(p._request,p._httpBackupApp,false,p._httpBackupBytes-p._request.length()-1,at,37);assert(part.length());out+=part;at+=part.length();}
 p.closeHttp();return out;
}
void post(ArdPortal& p,const char* secret){
 V body=V::object();body["page"]="secrets";body["values"]=V::object();body["values"]["token"]=secret;
 p._request=String("POST /api/app HTTP/1.1\r\n\r\n")+ArdJSON::JSON.stringify(body);
 assert(p.dynamicHttp("POST","/api/app"));assert(p._response.indexOf("202")>=0);p.closeHttp();drain(p);
}
int main(){
 ArdPortal p;assert(p.begin());drain(p);registerPage(p);
 auto field=p._dynamic.field("token");
 assert(ArdControl::secret(field) && !ArdControl::extended(field));
 assert(ArdControl::maxLength(field)==8192);
 assert(ArdHa::component(field).length()==0 && p._homeAssistant.discoveryConfig(field).length()==0);
 assert(ArdMatterFields::kind(field)==ArdMatterFields::Kind::Unsupported);
 assert(ArdZigbeeFields::describe(field,V("")).kind==ArdZigbeeFields::Kind::None);
 assert(p.getAppConfigValue("token").asString()=="firmware-secret");
 assert(p.portalAppValue("token").asString()=="");
 String definition=p._dynamic.pages.fieldHttpText(0,0);assert(definition.length() && definition.indexOf("firmware-secret")<0);
 auto document=ArdJSON::JSON.parse(definition);assert(document["default"].asString()=="");
 post(p,u8"hidden-żółć-🔒");
 assert(p.getAppConfigValue("token").asString()==u8"hidden-żółć-🔒");
 post(p,"");assert(p.getAppConfigValue("token").asString()==u8"hidden-żółć-🔒");
 p._request="GET /api/app HTTP/1.1\r\n\r\n";assert(p.dynamicHttp("GET","/api/app"));
 assert(p._response.indexOf("hidden-")<0 && p._response.indexOf("firmware-secret")<0);p.closeHttp();
 p._httpDynamicMode=2;p._httpDynamicStage=1;p._httpDynamicPageIndex=0;p._httpDynamicField=0;
 assert(p.buildDynamicHttpPart());assert(p._httpDynamicText.indexOf("hidden-")<0 && p._httpDynamicText.indexOf("\"token\":\"\"")>=0);p.closeHttp();
 // Direct commands/get/state publication cannot export or overwrite the secret.
 auto before=p.getAppConfigValue("token");
 p._appControls.dynamicMqtt("cmnd/"+ArdDeviceName::mqtt(p._config.deviceName)+"/token",reinterpret_cast<const uint8_t*>("bad"),3,false);
 assert(p.getAppConfigValue("token").asString()==before.asString());
 assert(!p._appControls.acceptMqttState("token","bad"));
 assert(!p.queueAppConfigStatePublish("token"));
 p._appControls._stateDirty=ArdAppConfigFieldMask::forField(0);
 assert(p._appControls.publishState() && !p._appControls._stateDirty.test(0));
 String file=backup(p);auto limits=ArdPortal::snapshotLimits();limits.maxInputBytes=ArdPortal::MaxBackupDocumentBytes;
 auto root=p._json.parse(file,nullptr,limits);assert(root.isValid());
 assert(root["maskedAppPasswords"][0].asString()=="token");
 assert(root["app"]["token"].asString()!=before.asString());
 assert(p.getAppConfigValue("token").asString()==before.asString());
 assert(ArdPortalJson::restoreAppPasswords(root));assert(root.length()==2 && root["app"]["token"].asString()==before.asString());
 auto legacy=ArdJSON::JSON.parse(R"({"config":{},"app":{"token":"old-plain-value"}})");
 assert(ArdPortalJson::restoreAppPasswords(legacy)&&legacy["app"]["token"].asString()=="old-plain-value");
 for(const char* invalid:{R"({"app":{"token":17},"maskedAppPasswords":["token"]})",R"({"app":{"token":"x"},"maskedAppPasswords":["missing"]})",R"({"app":{"token":"x","other":1},"maskedAppPasswords":["token","token"]})",R"({"app":{"token":"x"},"maskedAppPasswords":false})"}){
  auto bad=ArdJSON::JSON.parse(invalid);assert(!ArdPortalJson::restoreAppPasswords(bad));
 }
 // Masking can expand JSON escapes beyond the ordinary 8 KiB app budget.
 String escaped;for(unsigned i=0;i<5000;++i)escaped+='S';
 assert(p.setAppConfigValue("token",escaped));drain(p);assert(p.getAppConfigValue("token").asString()==escaped);
 String large=backup(p);assert(large.length()>ArdPortal::MaxAppConfigBytes);
 auto expanded=p._json.parse(large,nullptr,limits);assert(expanded.isValid());
 assert(ArdPortalJson::restoreAppPasswords(expanded) && expanded["app"]["token"].asString()==escaped);
 // Full restore consumes backup metadata before saving/restarting.
 post(p,"changed");
 p._http.endpoint=std::make_shared<Endpoint>();p._httpSince=fakeMillis;
 p._request=String("POST /api/restore HTTP/1.1\r\n\r\n")+file;
 assert(p.startConfigRestore(file.length(),p._request.indexOf("\r\n\r\n")));drain(p);
 assert(p.getAppConfigValue("token").asString()==before.asString());
 puts("PASS: local password, hidden HTTP/defaults, blank preservation, no MQTT/protocol entity and reversible portable backup/restore");
}
'''
# snapshotLimits is an implementation-local free function.
main = main.replace('ArdPortal::snapshotLimits()', 'snapshotLimits()')
with tempfile.TemporaryDirectory(prefix='ardportal-password-') as directory:
    work=Path(directory)
    (work/'test.cpp').write_text(fixture+main)
    subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-DESP8266','-DARDFS_HOST_TEST','-I'+str(root/'tests/memory-host'),'-I'+str(root/'src'),
                    str(work/'test.cpp'),*[str(root/'src'/name) for name in ('ArdStorage.cpp','JsonStream.cpp','JournalReader.cpp','JsonDocumentStream.cpp')],'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
