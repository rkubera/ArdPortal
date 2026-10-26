// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
// Preincluded only into Arduino's CPU-only Matter data-model allocator.
#include <sdkconfig.h>
#if defined(CONFIG_ARDPORTAL_MATTER_THREAD_WIFI) && CONFIG_ARDPORTAL_MATTER_THREAD_WIFI && defined(CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP) && CONFIG_ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP && (!defined(CONFIG_SPIRAM) || !CONFIG_SPIRAM)
#include <esp_attr.h>
#include <esp_heap_caps.h>
namespace ArdPortalMatterMemory {
/** @brief Prefer SDK-enabled RTC heap for CPU-only model data, preserving requested capabilities and normal fallback. */
inline IRAM_ATTR void* calloc(size_t count, size_t bytes, uint32_t caps) {
  if (!(caps & (MALLOC_CAP_DMA | MALLOC_CAP_EXEC | MALLOC_CAP_SPIRAM))) {
    return heap_caps_calloc_prefer(count, bytes, 2, uint32_t(caps | MALLOC_CAP_RTCRAM), caps);
  }
  return heap_caps_calloc(count, bytes, caps);
}
}
#define heap_caps_calloc ArdPortalMatterMemory::calloc
#endif
