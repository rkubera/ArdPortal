#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Exercise the real CPU-only RTC preference, fallback and allocation test override."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
heap=r'''
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#include <cassert>
constexpr uint32_t MALLOC_CAP_RTCRAM=1,MALLOC_CAP_INTERNAL=2,MALLOC_CAP_8BIT=4,MALLOC_CAP_DEFAULT=8;
inline int calls=0,mode=0;
inline void* heap_caps_malloc_prefer(size_t bytes,size_t count,...){
 ++calls;assert(count==2);va_list args;va_start(args,count);
 assert(va_arg(args,uint32_t)==(MALLOC_CAP_RTCRAM|MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
 assert(va_arg(args,uint32_t)==MALLOC_CAP_DEFAULT);va_end(args);
 // The SDK chooses the first pool, falls back to the second or returns nullptr.
 return mode==2?nullptr:std::malloc(bytes);
}
'''
source=r'''
#include <cassert>
#include "ArdAllocation.h"
#ifdef ARDPORTAL_TEST_ALLOCATE
int overridden=0;
void* testAllocate(size_t bytes) noexcept {++overridden;return std::malloc(bytes);}
#endif
struct Owned {int value;explicit Owned(int v):value(v){};};
int main(){
 auto p=ArdAllocation::create<Owned>(42);assert(p && p->value==42);ArdAllocation::Deleter<Owned>{}(p);
#if defined(ARDPORTAL_PREFER_RTC_HEAP) && !defined(ARDPORTAL_TEST_ALLOCATE)
 assert(calls==1);mode=1;p=ArdAllocation::create<Owned>(7);assert(p && p->value==7);ArdAllocation::Deleter<Owned>{}(p);
 mode=2;assert(!ArdAllocation::create<Owned>(1));assert(calls==3);
#endif
#ifdef ARDPORTAL_TEST_ALLOCATE
 assert(overridden==1);
#ifdef ARDPORTAL_PREFER_RTC_HEAP
 assert(!calls);
#endif
#endif
}
'''
with tempfile.TemporaryDirectory() as d:
 path=Path(d);(path/'esp_heap_caps.h').write_text(heap);(path/'test.cpp').write_text(source)
 for mixed,rtc,spiram,override in ((1,1,0,0),(1,1,0,1),(0,1,0,0),(1,0,0,0),(1,1,1,0)):
  (path/'sdkconfig.h').write_text((f'#define CONFIG_ARDPORTAL_MATTER_THREAD_WIFI {mixed}\n' if mixed else '')+(f'#define CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP {rtc}\n' if rtc else '')+('#define CONFIG_SPIRAM 1\n' if spiram else ''))
  flags=['-DARDPORTAL_TEST_ALLOCATE=testAllocate'] if override else []
  subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-exceptions','-DESP32','-DESP_PLATFORM',*flags,'-I'+d,'-I'+str(root/'src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
  subprocess.run([str(path/'test')],check=True)
print('PASS: actual RTC preference capabilities, fallback, failure, free/delete, test override and disabled/PSRAM profiles')
