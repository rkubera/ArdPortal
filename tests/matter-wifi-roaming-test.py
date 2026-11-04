#!/usr/bin/env python3
"""Check Matter source overlays, SDK feature guards and upstream-change rejection."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cmake = shutil.which('cmake')
assert cmake, 'CMake is required'
call = 'esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifiConfig);'
with tempfile.TemporaryDirectory(prefix='ardportal-matter-roaming-') as directory:
    base = Path(directory)
    for name, wifi, kv, ft, changed in (
        ('kvr', True, True, True, False),
        ('kv', True, True, False, False),
        ('r', True, False, True, False),
        ('disabled', True, False, False, False),
        ('thread', False, True, True, False),
        ('changed', True, True, True, True),
    ):
        project = base/name
        project.mkdir()
        for unit in ('NetworkCommissioningDriver', 'ESP32Utils'):
            (project/(unit+'.cpp')).write_text('int '+unit+'(){wifi_config_t wifiConfig={};'+
                ('return 0;' if changed else call+'return err;')+'}\n')
        (project/'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.20)
project(Roaming CXX)
add_library(sdk OBJECT NetworkCommissioningDriver.cpp ESP32Utils.cpp)
set(matter_lib sdk)
set(matter_sources "${{CMAKE_CURRENT_SOURCE_DIR}}/NetworkCommissioningDriver.cpp" "${{CMAKE_CURRENT_SOURCE_DIR}}/ESP32Utils.cpp")
set(CONFIG_ENABLE_WIFI_STATION {'TRUE' if wifi else 'FALSE'})
set(CONFIG_ESP_WIFI_11KV_SUPPORT {'TRUE' if kv else 'FALSE'})
set(CONFIG_ESP_WIFI_11R_SUPPORT {'TRUE' if ft else 'FALSE'})
include("{root}/idf/MatterPortal/MatterWiFiRoaming.cmake")
''')
        result = subprocess.run([cmake,'-S',str(project),'-B',str(project/'build')],capture_output=True,text=True)
        assert (result.returncode==0) != changed, result.stdout+result.stderr
        if changed:
            assert 'review the roaming overlay' in result.stderr
            continue
        overlay = project/'build/ardportal-matter-roaming'
        assert overlay.exists() == (wifi and (kv or ft))
        if not overlay.exists():
            continue
        stub = project/'stub.h'
        stub.write_text('''#include <cassert>
using esp_err_t=int; enum{WIFI_IF_STA=0};
struct wifi_config_t {struct {
#if CONFIG_ESP_WIFI_11KV_SUPPORT
int rm_enabled=0,btm_enabled=0;
#if CONFIG_ESP_WIFI_MBO_SUPPORT
int mbo_enabled=0;
#endif
#endif
#if CONFIG_ESP_WIFI_11R_SUPPORT
int ft_enabled=0;
#endif
bool bssid_set=true;int channel=6;
}sta;};
inline int esp_wifi_set_config(int,const wifi_config_t* c){
assert(!c->sta.bssid_set && c->sta.channel==0);
#if CONFIG_ESP_WIFI_11KV_SUPPORT
assert(c->sta.rm_enabled && c->sta.btm_enabled);
#if CONFIG_ESP_WIFI_MBO_SUPPORT
assert(c->sta.mbo_enabled);
#endif
#endif
#if CONFIG_ESP_WIFI_11R_SUPPORT
assert(c->sta.ft_enabled);
#endif
return 0;}
''')
        main = project/'main.cpp'
        main.write_text('int NetworkCommissioningDriver();int ESP32Utils();int main(){return NetworkCommissioningDriver()+ESP32Utils();}')
        for mbo in (0,1):
            binary=project/('test'+str(mbo))
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror',
                            '-DCONFIG_ESP_WIFI_11KV_SUPPORT='+str(int(kv)),
                            '-DCONFIG_ESP_WIFI_11R_SUPPORT='+str(int(ft)),
                            '-DCONFIG_ESP_WIFI_MBO_SUPPORT='+str(mbo),'-include',str(stub),
                            *map(str,overlay.glob('*.cpp')),str(main),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
        for unit in ('NetworkCommissioningDriver','ESP32Utils'):
            assert (project/(unit+'.cpp')).read_text().count(call)==1
print('PASS: Matter commission/restore overlays, partial SDK features, Wi-Fi-free exclusion and changed-source rejection')
