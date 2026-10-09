// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include <esp_heap_caps.h>
#elif defined(ESP8266)
#include <umm_malloc/umm_heap_select.h>
#endif
#ifndef ARDPORTAL_NETWORK_HEAP_RESERVE
#if defined(ESP32)
#define ARDPORTAL_NETWORK_HEAP_RESERVE 24576UL
#else
#define ARDPORTAL_NETWORK_HEAP_RESERVE 4096UL
#endif
#endif
#ifndef ARDPORTAL_NETWORK_DMA_RESERVE
#define ARDPORTAL_NETWORK_DMA_RESERVE 16384UL
#endif
#ifndef ARDPORTAL_NETWORK_BLOCK_RESERVE
#if defined(ESP32)
#define ARDPORTAL_NETWORK_BLOCK_RESERVE 4096UL
#else
#define ARDPORTAL_NETWORK_BLOCK_RESERVE 1024UL
#endif
#endif
namespace ArdHeap {
struct Snapshot {uint32_t free8,block8,total8,freeDma,blockDma;};
/** @brief Sample byte-addressable internal heap and its DMA subset.
 * @return Separate pool measurements; DMA is unavailable (zero) on ESP8266. */
inline Snapshot sample(){
#if defined(ESP32)
  constexpr uint32_t caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
  constexpr uint32_t dma=caps|MALLOC_CAP_DMA;
  return {uint32_t(heap_caps_get_free_size(caps)),uint32_t(heap_caps_get_largest_free_block(caps)),uint32_t(heap_caps_get_total_size(caps)),uint32_t(heap_caps_get_free_size(dma)),uint32_t(heap_caps_get_largest_free_block(dma))};
#else
#if defined(ESP8266)
  // Select DRAM even when the core also exposes an optional IRAM heap.
  // Use the allocator's configured arena, not nominal chip RAM or free-at-boot.
  HeapSelectDram dram;
  return {ESP.getFreeHeap(),ESP.getMaxFreeBlockSize(),uint32_t(UMM_MALLOC_CFG_HEAP_SIZE),0,0};
#else
  return {ESP.getFreeHeap(),ESP.getMaxFreeBlockSize(),0,0,0};
#endif
#endif
}
/** @brief Check headroom before starting an allocation-heavy cooperative unit.
 * @param bytes Estimated temporary allocation cost, including JSON members.
 * @return True when both free space and contiguous blocks preserve network reserves. */
inline bool permits(size_t bytes=2048,size_t contiguous=SIZE_MAX){
  if(contiguous==SIZE_MAX)contiguous=bytes;
  auto heap=sample();
  if(bytes>heap.free8||heap.free8-bytes<ARDPORTAL_NETWORK_HEAP_RESERVE||heap.block8<contiguous||heap.block8-contiguous<ARDPORTAL_NETWORK_BLOCK_RESERVE)return false;
#if defined(ESP32)
  if(bytes>heap.freeDma||heap.freeDma-bytes<ARDPORTAL_NETWORK_DMA_RESERVE||heap.blockDma<contiguous||heap.blockDma-contiguous<ARDPORTAL_NETWORK_BLOCK_RESERVE)return false;
#endif
  return true;
}
/** @brief Estimate a single field's peak parse/serialization working set.
 * @param bytes Length of the source JSON slice.
 * @return Conservative work estimate with space for members and output. */
inline size_t fieldWork(size_t bytes){return bytes> (SIZE_MAX-2048)/8?SIZE_MAX:bytes*8+2048;}
/** @brief Estimate a basic field from its registered JSON node count.
 * Source strings/output and allocator-backed members scale separately. Unknown
 * or extended definitions retain the conservative source-length estimate. */
inline size_t fieldWork(size_t bytes,size_t nodes,size_t memberBytes){
  if(!nodes)return fieldWork(bytes);
  if(bytes>(SIZE_MAX-1024)/2)return SIZE_MAX;
  const size_t text=bytes*2+1024;
  if(memberBytes&&nodes>(SIZE_MAX-text)/memberBytes)return SIZE_MAX;
  return text+nodes*memberBytes;
}
/** @brief Largest contiguous source/output buffer needed by a basic field. */
inline size_t fieldBlock(size_t bytes){return bytes>SIZE_MAX-512?SIZE_MAX:bytes+512;}
/** @brief Format a diagnostic sample without counting overlapping pools twice.
 * @param heap Snapshot to report.
 * @return Separate free/largest-block values for heap8 and DMA. */
inline String describe(const Snapshot& heap){return String("heap8=")+String(heap.free8)+" block8="+String(heap.block8)+" DMA="+String(heap.freeDma)+" blockDMA="+String(heap.blockDma);}
}
