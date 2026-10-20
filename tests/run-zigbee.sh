#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
work=$(mktemp -d /tmp/ardportal-zigbee-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I"$root/tests/zigbee-host" -I"$root/tests/host" -I"$root/src" "$root/tests/zigbee-host/lifecycle.cpp" -o "$work/test"
ASAN_OPTIONS=detect_leaks=0 "$work/test"
ASAN_OPTIONS=detect_leaks=0 "$work/test" saved-network
ASAN_OPTIONS=detect_leaks=0 "$work/test" time-service

for suite in fields native composite-native ota console converter time; do
  c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I"$root/tests/zigbee-host" -I"$root/tests/host" -I"$root/src" "$root/tests/zigbee-host/$suite.cpp" -o "$work/$suite"
  ASAN_OPTIONS=detect_leaks=0 "$work/$suite"
done

# Both chip identities use the same native mapping, with independent model labels.
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DCONFIG_IDF_TARGET_ESP32C5=1 -I"$root/tests/zigbee-host" -I"$root/tests/host" -I"$root/src" "$root/tests/zigbee-host/native.cpp" -o "$work/native-c5"
ASAN_OPTIONS=detect_leaks=0 "$work/native-c5"

# Debug output must not depend on a Console client or block on Serial capacity.
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -DARDPORTAL_ZIGBEE_DEBUG_SERIAL=1 -I"$root/tests/zigbee-host" -I"$root/tests/host" -I"$root/src" "$root/tests/zigbee-host/native.cpp" -o "$work/native-serial"
ASAN_OPTIONS=detect_leaks=0 "$work/native-serial"

c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -I"$root/tests/host" -I"$root/src" "$root/tests/zigbee-export-api-test.cpp" -o "$work/export-api"
ASAN_OPTIONS=detect_leaks=0 "$work/export-api"

python3 "$root/tests/zigbee-queue-order-test.py"
