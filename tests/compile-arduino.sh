#!/usr/bin/env bash
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
set -euo pipefail
cd "$(dirname "$0")/.."
repo_root="$PWD"
build_root=$(mktemp -d "${TMPDIR:-/tmp}/ardportal-builds.XXXXXX")
trap 'rm -rf "$build_root"' EXIT
cli="${ARDUINO_CLI:-arduino-cli}"

"$cli" compile --fqbn esp8266:esp8266:generic \
  --library "$repo_root" --build-path "$build_root/esp8266" examples/DynamicPages
"$cli" compile --fqbn esp32:esp32:esp32 \
  --library "$repo_root" --build-path "$build_root/esp32" examples/DynamicPages
for chip in esp32c5 esp32c6 esp32h2; do
  "$cli" compile --fqbn "esp32:esp32:${chip}:ZigbeeMode=ed,PartitionScheme=zigbee" \
    --library "$repo_root" --build-property upload.maximum_size=1835008 \
    --build-path "$build_root/$chip" examples/ZigbeeClimate
done
# Compile-time probes verify APIs disappear with their feature switches.
"${PYTHON:-python3}" tests/public-api-matrix.py "$build_root/api"
for sketch in "$build_root"/api/*; do
  "$cli" compile --fqbn esp8266:esp8266:generic \
    --library "$repo_root" --build-path "$build_root/api-build-$(basename "$sketch")" "$sketch"
done
