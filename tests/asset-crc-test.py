#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Compare the shared asset CRC combiner with zlib on actual byte streams."""
import os
from pathlib import Path
import random
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = r'''
#include <Arduino.h>
#include "ArdAsset.h"
#include <cstdio>
#include <cinttypes>
uint32_t fakeMillis = 0;
int main() {
  uint32_t left, right, length;
  while (std::scanf("%" SCNu32 " %" SCNu32 " %" SCNu32, &left, &right, &length) == 3)
    std::printf("%" PRIu32 "\n", ardAssetCrcAppend(left, right, length));
  for (uint32_t length : {0U, 1U, 255U, 256U, 4096U, 8192U}) {
    uint32_t crc = UINT32_MAX;
    for (uint32_t i = 0; i < length; ++i)
      crc = ArdCRC::updateByte(crc, uint8_t(i * 17 + 43));
    std::printf("%" PRIu32 "\n", ~crc);
  }
}
'''


def main():
    rng = random.Random(0xA4DF5)
    cases = []
    lengths = [(0, 0), (0, 1), (1, 0), (255, 256), (4096, 4097),
               (17, 65536), (8192, 65537)]
    lengths += [(rng.randrange(4097), rng.randrange(8193)) for _ in range(128)]
    for before, after in lengths:
        left = bytes(rng.getrandbits(8) for _ in range(before))
        right = bytes(rng.getrandbits(8) for _ in range(after))
        cases.append((zlib.crc32(left), zlib.crc32(right), after,
                      zlib.crc32(right, zlib.crc32(left))))
    # This public helper historically XORs arbitrary CRCs at length zero.
    for _ in range(32):
        left, right = rng.getrandbits(32), rng.getrandbits(32)
        cases.append((left, right, 0, left ^ right))
    with tempfile.TemporaryDirectory(prefix='ardportal-asset-crc-') as temp:
        source = Path(temp) / 'crc.cpp'
        binary = Path(temp) / 'crc'
        source.write_text(FIXTURE)
        subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall',
                        '-Wextra', '-Werror', '-fsanitize=address,undefined',
                        '-I' + str(ROOT / 'tests/host'), '-I' + str(ROOT / 'src'),
                        str(source), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], input=''.join(
            f'{left} {right} {length}\n' for left, right, length, _ in cases),
            text=True, capture_output=True, check=True)
        actual = [int(value) for value in result.stdout.splitlines()]
        expected = [value for _, _, _, value in cases]
        expected += [zlib.crc32(bytes((i * 17 + 43) & 255 for i in range(length)))
                     for length in (0, 1, 255, 256, 4096, 8192)]
        assert actual == expected, 'CRC combination differs from zlib or zero-length contract'
    print(f'PASS: shared asset CRC combination, {len(cases) + 6} zlib/boundary cases with ASan/UBSan')


if __name__ == '__main__':
    main()
