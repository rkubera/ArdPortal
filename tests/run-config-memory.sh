#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/ardportal-config-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
for test_name in json-patch stream dynamic-patch no-dynamic; do
  source_files=("tests/memory-host/$test_name.cpp")
  flags=(-Itests/memory-host -Isrc)
  if [[ "$test_name" != json-patch ]]; then
    source_files+=(src/ArdFS.cpp)
    flags+=(-DESP8266 -DARDFS_HOST_TEST)
  fi
  "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined "${flags[@]}" -Itests/memory-host -Isrc "${source_files[@]}" -o "$test_dir/$test_name"
  "$test_dir/$test_name"
done
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DESP8266 -Itests/memory-host -Isrc tests/memory-host/ardfs.cpp src/ArdFS.cpp src/ArdFSVolume.cpp -o "$test_dir/power-cut"
"$test_dir/power-cut"
echo 'PASS: incremental settings, Portal/MQTT persistence, bounded journal writes and recovery'
