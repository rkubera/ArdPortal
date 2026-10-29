#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/ardportal-littlefs-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
sources=(src/ArdStorage.cpp src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp)
flags=(-std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/littlefs-host -Itests/memory-host -Isrc)
for platform in ESP8266 ESP32; do
  "${CXX:-c++}" "${flags[@]}" -D"$platform" tests/littlefs-host/storage.cpp "${sources[@]}" -o "$test_dir/storage-$platform"
  "${CXX:-c++}" "${flags[@]}" -D"$platform" -MM tests/littlefs-host/storage.cpp > "$test_dir/storage-$platform.d"
  "${PYTHON:-python3}" - "$test_dir/storage-$platform.d" <<'PYTEST'
import sys
from pathlib import Path
text = Path(sys.argv[1]).read_text()
assert not any('/' + name in text for name in ('ArdFS.h', 'ArdFSVolume.h', 'ArdFSVolumeImpl.h')), 'LittleFS includes the native ArdFS backend'
PYTEST
  "$test_dir/storage-$platform"
done
"${CXX:-c++}" "${flags[@]}" -DESP8266 tests/littlefs-host/portal.cpp "${sources[@]}" -o "$test_dir/portal"
"$test_dir/portal"
echo 'PASS: Portal config and AppConfig save/reload through LittleFS with a sketch-local define'
