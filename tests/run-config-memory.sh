#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/ardportal-config-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
for test_name in json-patch journal-selection stream dynamic-patch no-dynamic no-www no-www-wifi messages-only; do
  source_name="$test_name"
  if [[ "$test_name" == no-www || "$test_name" == no-www-wifi || "$test_name" == messages-only ]]; then source_name=no-dynamic; fi
  source_files=("tests/memory-host/$source_name.cpp")
  flags=(-Itests/memory-host -Isrc)
  if [[ "$test_name" == journal-selection ]]; then flags+=(-DARDPORTAL_TEST_ALLOCATE=testAllocate); fi
  if [[ "$test_name" == no-www ]]; then flags+=(-DARDPORTAL_ENABLE_WIFI=0 -DARDPORTAL_ENABLE_WWW=0); fi
  if [[ "$test_name" == no-www-wifi ]]; then flags+=(-DARDPORTAL_ENABLE_WWW=0); fi
  if [[ "$test_name" == messages-only ]]; then flags+=(-DARDPORTAL_ENABLE_CONSOLE_MESSAGES=1); fi
  if [[ "$test_name" != json-patch ]]; then
    source_files+=(src/ArdFS.cpp)
    flags+=(-DESP8266 -DARDFS_HOST_TEST)
  fi
  "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined "${flags[@]}" -Itests/memory-host -Isrc "${source_files[@]}" -o "$test_dir/$test_name"
  "$test_dir/$test_name"
done
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DESP8266 -Itests/memory-host -Isrc tests/memory-host/ardfs.cpp src/ArdFS.cpp src/ArdFSVolume.cpp -o "$test_dir/power-cut"
"$test_dir/power-cut"
for enabled in 0 1; do
  "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DARDPORTAL_ENABLE_TEXT_TYPES="$enabled" -Itests/host -Isrc tests/text-types-test.cpp -o "$test_dir/text-types-$enabled"
  "$test_dir/text-types-$enabled"
done
echo 'PASS: incremental settings, Portal/MQTT persistence, bounded journal writes and recovery'
