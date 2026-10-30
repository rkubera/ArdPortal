#!/usr/bin/env python3
"""Compile and decompress actual emitted C++ assets for all control profiles."""
from pathlib import Path
import subprocess,sys,tempfile,zlib
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'src/tools'))
from control_assets import CONTROLS,GROUPS,emit_profile_chunks,selected
from generate_portal_assets import portal_parts,gzip_array
_,script,_=portal_parts(root/'src',ota=False,mqtt=False,guarded=True)
assets='\n'.join(emit_profile_chunks('TEST_JS',script,gzip_array))
fixture='#include <Arduino.h>\n#include "ArdAsset.h"\n#include <cstdio>\nuint32_t fakeMillis=0;\n'+assets+'''
int main(){for(const auto& chunk:TEST_JS)std::fwrite(chunk.data,1,chunk.size,stdout);}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-profile-assets-') as directory:
 path=Path(directory);cpp=path/'test.cpp';cpp.write_text(fixture)
 for profile,controls in (('FULL',None),('BASIC',['slider','text','select','edit']),('CUSTOM',['switch','slider','text'])):
  for zigbee,matter,traffic,groups,dependencies in ((0,0,0,0,1),(1,0,0,0,1),(0,1,0,0,1),(0,1,1,0,1),(0,0,0,1,1),(0,0,0,1,0)):
   for visibility in (0,1):
    features={'BACKUP_RESTORE':visibility,'VISIBILITY':visibility,'GROUPS':groups,'DYNAMIC_PAGES':1,'DEPENDENCIES':dependencies,'ZIGBEE':zigbee,'MATTER':matter,'MATTER_CONSOLE':traffic}
    defines={'ARDPORTAL_ENABLE_'+k:v for k,v in features.items()}
    defines.update({'ARDPORTAL_CONTROL_PROFILE_FULL':int(profile=='FULL'),'ARDPORTAL_CONTROL_PROFILE_BASIC':int(profile=='BASIC')})
    defines.update({'ARDPORTAL_ENABLE_CONTROL_'+c.upper():int(controls is None or c in controls) for c in CONTROLS})
    defines.update({'ARDPORTAL_CONTROL_SUPPORT_'+group:int(any(controls is None or c in controls for c in members)) for group,members in GROUPS.items()})
    subprocess.run(['c++','-std=c++17','-I'+str(root/'tests/host'),'-I'+str(root/'src'),*['-D'+k+'='+str(v)for k,v in defines.items()],str(cpp),'-o',str(path/'test')],check=True)
    data=subprocess.check_output([str(path/'test')])
    compiled=zlib.decompressobj(31).decompress(data).decode()
    expected=selected(script,controls,features)
    assert ("/api/groups" in compiled)==bool(groups),(profile,features)
    assert ('function previewDynamicVisibility(' in compiled)==bool(dependencies),(profile,features)
    assert ('/api/backup' in compiled)==bool(visibility),(profile,features)
    assert compiled==expected,(profile,features,'emitted asset differs from selected source')
    assert ('function openMatterConsole(' in compiled)==bool(traffic),(profile,features)
    assert ('function appendConsoleLine(' in compiled)==bool(traffic or zigbee),(profile,features)
print('PASS: actual C++ gzip assets for FULL/BASIC/CUSTOM, Matter console on/off, Zigbee and shared renderer')
