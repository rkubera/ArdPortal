#!/usr/bin/env python3
"""Exercise production provisioning admission and profile-dependent option storage."""
import re
import os
import subprocess
import tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
header=r'''
#pragma once
#include <cstdint>
#include <cstddef>
using nvs_handle_t=unsigned;
constexpr int ESP_OK=0,NVS_READONLY=1;
int nvs_flash_init_partition(const char*);
int nvs_open_from_partition(const char*,const char*,int,nvs_handle_t*);
int nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
int nvs_get_str(nvs_handle_t,const char*,char*,size_t*);
int nvs_get_u32(nvs_handle_t,const char*,uint32_t*);
void nvs_close(nvs_handle_t);
'''
fixture=r'''
#include <cassert>
#include <string>
#include "MatterFactory.h"
bool initialized=true,namespacePresent=true;std::string missing,empty;int closes=0;
int nvs_flash_init_partition(const char* name){assert(std::string(name)=="fctry");return initialized?0:-1;}
int nvs_open_from_partition(const char* partition,const char* ns,int mode,nvs_handle_t* h){assert(std::string(partition)=="fctry");assert(std::string(ns)=="chip-factory");assert(mode==NVS_READONLY);*h=1;return namespacePresent?0:-1;}
int nvs_get_blob(nvs_handle_t,const char* key,void* buffer,size_t* length){assert(!buffer);*length=empty==key?0:32;return missing==key?-1:0;}
int nvs_get_str(nvs_handle_t,const char* key,char* buffer,size_t* length){assert(!buffer);*length=empty==key?1:32;return missing==key?-1:0;}
int nvs_get_u32(nvs_handle_t,const char* key,uint32_t* value){*value=1;return missing==key?-1:0;}
void nvs_close(nvs_handle_t){++closes;}
int main(){
 initialized=false;assert(!ArdMatterFactory::ready());assert(closes==0);
 initialized=true;namespacePresent=false;assert(!ArdMatterFactory::ready());assert(closes==0);
 namespacePresent=true;assert(ArdMatterFactory::ready());assert(closes==1);
 for(const char* key:{"dac-cert","dac-key","dac-pub-key","pai-cert","cert-dclrn","salt","verifier","vendor-name","product-name","discriminator","iteration-count","vendor-id","product-id","hardware-ver"}){missing=key;const int before=closes;assert(!ArdMatterFactory::ready());assert(closes==before+1);}
 missing="";
 for(const char* key:{"dac-cert","dac-key","dac-pub-key","pai-cert","cert-dclrn","salt","verifier","vendor-name","product-name"}){empty=key;assert(!ArdMatterFactory::ready());}
 empty="";assert(ArdMatterFactory::ready());
}
'''
options=re.search(r'  struct Options \{.*?\n  \};',(root/'src/ArdMatter.h').read_text(),re.S)[0]
with tempfile.TemporaryDirectory(prefix='ardportal-credentials-') as directory:
 p=Path(directory);(p/'nvs.h').write_text(header);(p/'nvs_flash.h').write_text('#include "nvs.h"\n');(p/'test.cpp').write_text(fixture)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),'-I'+str(root/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 for production in (0,1):
  check='static_assert(sizeof(Options)==1);' if production else 'static_assert(Options{}.passcode==20202021 && Options{}.discriminator==3840);'
  (p/'options.cpp').write_text('#include <cstdint>\n'+f'#define ARDPORTAL_MATTER_PRODUCTION {production}\n'+options+'\n'+check+'\nint main(){}\n')
  subprocess.run(['c++','-std=c++17',str(p/'options.cpp'),'-o',str(p/'options')],check=True)
# Both maps retain all app/ArdFS locations; factory NVS replaces an unused
# coredump slot without overlapping mutable runtime NVS.
def partitions(name):
 rows={}
 for line in (root/'idf/MatterPortal'/name).read_text().splitlines():
  if line.strip() and not line.startswith('#'):
   row=[v.strip() for v in line.split(',')];rows[row[0]]=row[1:]
 return rows
original=partitions('partitions.csv');production=partitions('partitions.production.csv')
for name in ('nvs','phy_init','factory','spiffs'):assert production[name]==original[name]
assert production['fctry'][:4]==['data','nvs','0x3F0000','0x10000']
assert 'coredump' not in production
print('PASS: production fails closed for missing factory records, preserves keys and has no development PIN options; partition offsets stable')

# Execute the actual CMake credential argument parser without loading an SDK.
cmake=(root/'idf/MatterPortal/CMakeLists.txt').read_text()
a=cmake.index('set(ARDPORTAL_CREDENTIALS ')
b=cmake.index('get_filename_component(ARDPORTAL_ROOT',a)
with tempfile.TemporaryDirectory(prefix='ardportal-profile-choice-') as directory:
 p=Path(directory)/'choice.cmake'
 p.write_text(cmake[a:b]+'\nif(NOT ARDPORTAL_CREDENTIALS STREQUAL EXPECTED)\nmessage(FATAL_ERROR "Unexpected default profile")\nendif()\n')
 for supplied,expected in ((None,'development'),('development','development'),('production','production'),('invalid','invalid')):
  args=[os.environ.get('CMAKE','cmake'),'-DEXPECTED='+expected]+(['-DARDPORTAL_CREDENTIALS='+supplied] if supplied else [])+['-P',str(p)]
  result=subprocess.run(args,capture_output=True,text=True)
  assert (result.returncode==0)==(supplied!='invalid'),result.stderr
print('PASS: development is the default credential profile; production is explicit and invalid selections fail')

# Execute the actual fail-closed CMake checks against both default fragments.
guards=cmake[cmake.index('# Credential selection is independent of transport.'):cmake.index('# Thread role guards:')]
with tempfile.TemporaryDirectory(prefix='ardportal-provider-guards-') as directory:
 p=Path(directory)/'providers.cmake'
 for stored in ('development','production'):
  values={}
  for line in (root/'idf/MatterPortal'/('sdkconfig.credentials.'+stored)).read_text().splitlines():
   if line.startswith('CONFIG_'):
    key,value=line.split('=',1);values[key]='TRUE' if value=='y' else 'FALSE' if value=='n' else value
  for requested in ('development','production'):
   p.write_text('\n'.join(f'set({key} {value})' for key,value in values.items())+'\nset(ARDPORTAL_CREDENTIALS '+requested+')\n'+guards)
   result=subprocess.run([os.environ.get('CMAKE','cmake'),'-P',str(p)],capture_output=True,text=True)
   assert (result.returncode==0)==(stored==requested),result.stderr
  if stored=='production':
   for key,value in {'CONFIG_EXAMPLE_DAC_PROVIDER':'TRUE','CONFIG_ENABLE_TEST_SETUP_PARAMS':'TRUE','CONFIG_FACTORY_PARTITION_DAC_PROVIDER':'FALSE','CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL':'"nvs"','CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH':'TRUE'}.items():
    changed={**values,key:value}
    p.write_text('\n'.join(f'set({k} {v})' for k,v in changed.items())+'\nset(ARDPORTAL_CREDENTIALS production)\n'+guards)
    result=subprocess.run([os.environ.get('CMAKE','cmake'),'-P',str(p)],capture_output=True,text=True)
    assert result.returncode!=0,(key,result.stdout,result.stderr)
print('PASS: stale profiles, test fallback, wrong factory partition and coredump overlap rejected by actual CMake guards')
