#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Exercise the exact SDK data-model allocation policy without affecting DMA memory."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
heap=r'''
#pragma once
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdarg>
constexpr uint32_t MALLOC_CAP_RTCRAM=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_8BIT=4,MALLOC_CAP_DMA=8,MALLOC_CAP_EXEC=16,MALLOC_CAP_SPIRAM=32;
inline int direct=0,preferred=0;inline bool fail=false;
inline void* heap_caps_calloc(size_t n,size_t bytes,uint32_t){++direct;return fail?nullptr:std::calloc(n,bytes);}
inline void* heap_caps_calloc_prefer(size_t n,size_t bytes,size_t count,...){
 ++preferred;assert(count==2);va_list args;va_start(args,count);auto rtc=va_arg(args,uint32_t),fallback=va_arg(args,uint32_t);va_end(args);
 assert(rtc==(fallback|MALLOC_CAP_RTCRAM));assert(!(rtc&(MALLOC_CAP_DMA|MALLOC_CAP_EXEC|MALLOC_CAP_SPIRAM)));
 return fail?nullptr:std::calloc(n,bytes);
}
'''
source=r'''
#include <cassert>
#include <cstdlib>
#include "MatterMemoryPolicy.h"
#include <esp_heap_caps.h>
int main(){
 auto p=static_cast<unsigned char*>(heap_caps_calloc(3,17,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));assert(p);
 for(unsigned i=0;i<51;++i){assert(!p[i]);}
 std::free(p);
#if EXPECT_POLICY
 assert(preferred==1 && direct==0);
#else
 assert(preferred==0 && direct==1);
#endif
 for(auto caps:{MALLOC_CAP_DMA,MALLOC_CAP_EXEC,MALLOC_CAP_SPIRAM}){
  auto before=direct;p=static_cast<unsigned char*>(heap_caps_calloc(1,8,caps));assert(p && direct==before+1);std::free(p);
 }
 fail=true;assert(!heap_caps_calloc(2,8,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
}
'''.replace('#include <cstdlib>','#include <cstdlib>\n#include <initializer_list>')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'esp_heap_caps.h').write_text(heap);(p/'esp_attr.h').write_text('#define IRAM_ATTR\n');(p/'test.cpp').write_text(source)
 for mixed,rtc,spiram in ((1,1,0),(0,1,0),(1,0,0),(1,1,1)):
  (p/'sdkconfig.h').write_text(f'#define CONFIG_ARDPORTAL_MATTER_THREAD_WIFI {mixed}\n#define CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP {rtc}\n#define CONFIG_SPIRAM {spiram}\n')
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-exceptions',f'-DEXPECT_POLICY={int(bool(mixed and rtc and not spiram))}','-I'+d,'-I'+str(root/'src'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('PASS: actual SDK CPU-model RTC preference, zero initialization, DMA/EXEC/SPIRAM bypass, failure and disabled profiles')
