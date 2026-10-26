#!/usr/bin/env bash
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
set -euo pipefail
cd "$(dirname "$0")/.."
repo_root="$PWD"
build_root=$(mktemp -d "${TMPDIR:-/tmp}/ardportal-builds.XXXXXX")
trap 'rm -rf "$build_root"' EXIT
cli="${ARDUINO_CLI:-arduino-cli}"
# ESP8266 core 3.1.2 elf2bin.py uses a valid regex in a non-raw string.
# Python 3.12 warns about its \s escape; retain all other Python/compiler warnings.
esp8266_python_warnings="ignore:invalid escape sequence '\\s':SyntaxWarning${PYTHONWARNINGS:+,$PYTHONWARNINGS}"

env PYTHONWARNINGS="$esp8266_python_warnings" "$cli" compile --fqbn esp8266:esp8266:generic \
  --library "$repo_root" --build-path "$build_root/esp8266" examples/DynamicPages
"$cli" compile --fqbn esp32:esp32:esp32 \
  --library "$repo_root" --build-path "$build_root/esp32" examples/DynamicPages
for chip in esp32c5 esp32c6 esp32h2; do
  "$cli" compile --fqbn "esp32:esp32:${chip}:ZigbeeMode=ed,PartitionScheme=zigbee" \
    --library "$repo_root" --build-property upload.maximum_size=1835008 \
    --build-path "$build_root/$chip" examples/ZigbeeClimate
done
# Matter uses a custom 3.5 MiB single-app partition table; no firmware OTA.
for transport in 0 1; do
  "$cli" compile --fqbn esp32:esp32:esp32c6:PartitionScheme=huge_app \
    --library "$repo_root" --build-property upload.maximum_size=3670016 \
    --build-property "compiler.cpp.extra_flags=-DARDPORTAL_MATTER_THREAD=$transport" \
    --build-path "$build_root/matter-c6-$transport" examples/MatterPortal
done
"$cli" compile --fqbn esp32:esp32:esp32h2:PartitionScheme=huge_app \
  --library "$repo_root" --build-property upload.maximum_size=3670016 \
  --build-property compiler.cpp.extra_flags=-DARDPORTAL_MATTER_THREAD=1 \
  --build-path "$build_root/matter-h2" examples/MatterPortal
"$cli" compile --fqbn esp32:esp32:esp32c6:PartitionScheme=huge_app \
  --library "$repo_root" --build-property upload.maximum_size=3670016 \
  --build-property compiler.cpp.extra_flags=-DARDPORTAL_ENABLE_CONTROLS=1 \
  --build-path "$build_root/matter-full" examples/MatterPortal
# Compile-time probes verify APIs disappear with their feature switches.
"${PYTHON:-python3}" tests/public-api-matrix.py "$build_root/api"
# The network-free ESP32 profile must not retain the native Wi-Fi driver.
"$cli" compile --fqbn esp32:esp32:esp32 \
  --library "$repo_root" --build-path "$build_root/esp32-no-network" "$build_root/api/no_network"
"${PYTHON:-python3}" - "$build_root/esp32-no-network/no_network.ino.map" <<'PY'
import re
import sys
from pathlib import Path

link_map = Path(sys.argv[1]).read_text()
assert not re.search(r'^\s+0x[0-9a-fA-F]+\s+esp_wifi_\w+\s*$', link_map, re.MULTILINE), \
    'Network-free ESP32 firmware unexpectedly retains Wi-Fi driver symbols'
print('PASS: network-free ESP32 firmware excludes Wi-Fi driver symbols')
PY
for sketch in "$build_root"/api/*; do
  env PYTHONWARNINGS="$esp8266_python_warnings" "$cli" compile --fqbn esp8266:esp8266:generic \
    --library "$repo_root" --build-path "$build_root/api-build-$(basename "$sketch")" "$sketch"
done
