// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "ArdCRC.h"

// Compiler-selected independent DEFLATE blocks share one gzip header/footer.
static const char ARD_ASSET_GZIP_HEAD[] PROGMEM = "\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff";
static const char ARD_ASSET_DEFLATE_END[] PROGMEM = "\x03\x00";
struct ArdAssetChunk {
  const char* data;
  size_t size;
  uint32_t crc = 0, plainSize = 0;
};
/**
 * @brief Read a chunk descriptor from program memory.
 * @param chunks Chunked immutable asset descriptor.
 * @param index Zero-based element or field index.
 * @return The descriptor copied into RAM.
 */
inline ArdAssetChunk ardAssetChunk(const ArdAssetChunk* chunks, size_t index) {
  ArdAssetChunk value;
  memcpy_P(&value, chunks + index, sizeof(value));
  return value;
}
/**
 * @brief Sum the compressed byte lengths of all selected asset chunks.
 * @param chunks Chunked immutable asset descriptor.
 * @param count Number of items or bytes to process.
 * @return Total asset byte length.
 */
inline size_t ardAssetSize(const ArdAssetChunk* chunks, size_t count) {
  size_t size = 0;
  for (size_t i = 0; i < count; ++i) size += ardAssetChunk(chunks, i).size;
  return size;
}
// Polynomial multiplication in the reflected CRC-32 representation.
/**
 * @brief Multiply two reflected CRC-32 polynomials.
 * @param a First reflected polynomial.
 * @param b Second reflected polynomial.
 * @return The polynomial product modulo the CRC-32 polynomial.
 */
inline uint32_t ardAssetCrcProduct(uint32_t a, uint32_t b) {
  return ArdCRC::multiply(a, b);
}
/**
 * @brief Combine the CRC of an appended chunk with the existing asset CRC.
 * @param crc Running CRC value to combine with the new bytes.
 * @param next CRC-32 of the appended chunk.
 * @param bytes Uncompressed byte length of the appended chunk.
 * @return CRC-32 of the concatenated uncompressed data.
 */
inline uint32_t ardAssetCrcAppend(uint32_t crc, uint32_t next, uint32_t bytes) {
  // Preserve the helper's zero-length XOR behavior for arbitrary inputs.
  return bytes ? ArdCRC::combine(crc, next, bytes) : crc ^ next;
}
