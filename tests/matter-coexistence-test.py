#!/usr/bin/env python3
"""Exercise the actual mixed-radio startup gate, including SDK failures."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/MatterNativeImpl.h').read_text()
a = source.index('  // OpenThread initialization alone does not enable')
b = source.index('  _running = true;', a) + len('  _running = true;')
assert source.index('  Matter.begin();') < source.index('  if (!SDK::ready())') < a
fixture = r'''
#include <cassert>
#include <string>
constexpr int ESP_OK=0;
int calls=0, result=ESP_OK;
int esp_coex_wifi_i154_enable(){++calls;return result;}
struct Owner {
 bool _running=false;std::string error;
 void fail(const char* value){error=value;}
 bool start(){
'''
main = r'''
 return true;}
};
int main(){
 for(int sdkResult:{ESP_OK,-7}){
  calls=0;result=sdkResult;Owner owner;
  constexpr bool mixed=ARDPORTAL_MATTER_THREAD && ARDPORTAL_ENABLE_WIFI;
  constexpr bool supported=CONFIG_ESP_COEX_SW_COEXIST_ENABLE && CONFIG_SOC_IEEE802154_SUPPORTED;
  const bool expected=!mixed || (supported && sdkResult==ESP_OK);
  assert(owner.start()==expected);
  assert(owner._running==expected);
  assert(calls==int(mixed && supported));
  assert(owner.error.empty()==expected);
 }
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.cpp').write_text(fixture + source[a:b] + main)
    for thread in (0, 1):
        for wifi in (0, 1):
            for coex in (0, 1):
                for radio in (0, 1):
                    flags = [f'-D{name}={value}' for name, value in (
                        ('ARDPORTAL_MATTER_THREAD', thread), ('ARDPORTAL_ENABLE_WIFI', wifi),
                        ('CONFIG_ESP_COEX_SW_COEXIST_ENABLE', coex), ('CONFIG_SOC_IEEE802154_SUPPORTED', radio))]
                    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                                    '-fsanitize=address,undefined', *flags, str(path / 'test.cpp'),
                                    '-o', str(path / 'test')], check=True)
                    subprocess.run([str(path / 'test')], check=True)
print('PASS: actual Matter coexistence startup call, SDK failure handling and all radio/profile guards')
