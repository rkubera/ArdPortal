#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/ardportal-config-tests.XXXXXX")"
trap 'rm -rf "$test_dir"' EXIT
"${PYTHON:-python3}" tests/page-limit-config-test.py
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Itests/memory-host -Isrc tests/memory-host/json-utf8.cpp src/JsonStream.cpp -o "$test_dir/json-utf8"
"$test_dir/json-utf8"
for test_name in json-patch json-stream journal-selection stream dynamic-patch dynamic-groups dynamic-page-limit-default dynamic-page-limit-32 dynamic-page-limit-groups mqtt-state-topic mqtt-state-topic-no-ha no-dynamic no-www no-www-wifi messages-only; do
  source_name="$test_name"
  if [[ "$test_name" == no-www || "$test_name" == no-www-wifi || "$test_name" == messages-only ]]; then source_name=no-dynamic; fi
  if [[ "$test_name" == mqtt-state-topic-no-ha ]]; then source_name=mqtt-state-topic; fi
  source_files=("tests/memory-host/$source_name.cpp")
  flags=(-Itests/memory-host -Isrc)
  if [[ "$test_name" == dynamic-groups ]]; then source_name=dynamic-patch; source_files=("tests/memory-host/$source_name.cpp"); flags+=(-DARDPORTAL_ENABLE_GROUPS=1); fi
  if [[ "$test_name" == dynamic-page-limit-* ]]; then
    source_files=(tests/memory-host/dynamic-patch.cpp)
    flags+=(-DARDPORTAL_TEST_PAGE_LIMIT=1)
    if [[ "$test_name" != dynamic-page-limit-default ]]; then flags+=(-DARDPORTAL_APP_CONFIG_MAX_PAGES=32); fi
    if [[ "$test_name" == dynamic-page-limit-groups ]]; then flags+=(-DARDPORTAL_ENABLE_GROUPS=1); fi
  fi
  if [[ "$test_name" == journal-selection || "$test_name" == json-patch || "$source_name" == dynamic-patch ]]; then flags+=(-DARDPORTAL_TEST_ALLOCATE=testAllocate); fi
  if [[ "$test_name" == mqtt-state-topic-no-ha ]]; then flags+=(-DARDPORTAL_ENABLE_HA=0); fi
  if [[ "$test_name" == no-www ]]; then flags+=(-DARDPORTAL_ENABLE_WIFI=0 -DARDPORTAL_ENABLE_WWW=0); fi
  if [[ "$test_name" == no-www-wifi ]]; then flags+=(-DARDPORTAL_ENABLE_WWW=0); fi
  if [[ "$test_name" == messages-only ]]; then flags+=(-DARDPORTAL_ENABLE_CONSOLE_MESSAGES=1); fi
  if [[ "$test_name" != json-patch && "$test_name" != json-stream ]]; then
    source_files+=(src/ArdStorage.cpp)
    flags+=(-DESP8266 -DARDFS_HOST_TEST)
  fi
  source_files+=(src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp)
  "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined "${flags[@]}" -Itests/memory-host -Isrc "${source_files[@]}" -o "$test_dir/$test_name"
  "$test_dir/$test_name"
done
for dependencies in 0 1; do
  for visibility in 0 1; do
    "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
      -DESP8266 -DARDFS_HOST_TEST -DARDPORTAL_TEST_ALLOCATE=testAllocate -DARDPORTAL_TEST_VISIBILITY \
      -DARDPORTAL_ENABLE_DEPENDENCIES="$dependencies" -DARDPORTAL_ENABLE_VISIBILITY="$visibility" \
      -Itests/memory-host -Isrc tests/memory-host/dynamic-patch.cpp src/ArdStorage.cpp src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp -o "$test_dir/visibility"
    "$test_dir/visibility"
  done
done
for www in 0 1; do
  for mqtt in 0 1; do
    "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
      -DESP8266 -DARDFS_HOST_TEST -DARDPORTAL_ENABLE_WWW=$www -DARDPORTAL_ENABLE_MQTT=$mqtt \
      -Itests/memory-host -Isrc tests/memory-host/log-callback.cpp src/ArdStorage.cpp src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp -o "$test_dir/log-callback"
    "$test_dir/log-callback"
  done
done
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DESP8266 -Itests/memory-host -Isrc tests/memory-host/ardfs.cpp src/ArdStorage.cpp src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp -o "$test_dir/power-cut"
"$test_dir/power-cut"
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -DESP8266 -DFS_PHYS_SIZE=262144U -Itests/memory-host -Isrc tests/memory-host/raw-file.cpp src/ArdStorage.cpp src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp -o "$test_dir/raw-file"
"$test_dir/raw-file"
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -DESP8266 -DFS_PHYS_SIZE=262144U -Itests/memory-host -Isrc tests/memory-host/large-journal.cpp src/ArdStorage.cpp src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp -o "$test_dir/large-journal"
"$test_dir/large-journal"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DESP8266 -Itests/memory-host -Isrc tests/memory-host/async-storage-example.cpp src/ArdStorage.cpp src/JsonStream.cpp src/JournalReader.cpp src/JsonDocumentStream.cpp -o "$test_dir/async-storage-example"
"$test_dir/async-storage-example"
for enabled in 0 1; do
  "${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DARDPORTAL_ENABLE_CLASSES="$enabled" -Itests/host -Isrc tests/control-classes-test.cpp src/JsonStream.cpp src/JsonDocumentStream.cpp -o "$test_dir/control-classes-$enabled"
  "$test_dir/control-classes-$enabled"
done
"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DESP8266 -DFS_PHYS_SIZE=524288U -DARDPORTAL_TEST_ALLOCATE=testAllocate \
  -Itests/memory-host -Isrc tests/memory-host/volume-index.cpp -o "$test_dir/volume-index"
"$test_dir/volume-index"
echo 'PASS: incremental settings, Portal/MQTT persistence, bounded journal writes and recovery'

"${CXX:-c++}" -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DESP8266 -DFS_PHYS_SIZE=524288U -DARDPORTAL_TEST_ALLOCATE=testAllocate \
  -Itests/memory-host -Isrc tests/memory-host/volume-operations.cpp -o "$test_dir/volume-operations"
"$test_dir/volume-operations"
