#!/usr/bin/env bash
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
set -euo pipefail
cd "$(dirname "$0")/.."
profiles=(portal-esp8266 portal-esp32 zigbee-c5-ed zigbee-c6-ed zigbee-h2-ed
  matter-c6-wifi matter-c6-thread matter-h2-thread matter-c6-full no-network-esp32
  api-all api-no_power api-no_www api-log_headless api-log_without_console
  api-no_network api-no_dynamic api-no_actions api-local_dynamic)
if [[ $# -gt 1 ]]; then
  printf 'Usage: %s [all|--list|PROFILE]\n' "$0" >&2
  exit 2
fi
profile="${1:-all}"
if [[ "$profile" == --list ]]; then
  printf '%s\n' "${profiles[@]}"
  exit 0
fi
if [[ "$profile" == all ]]; then
  for selected in "${profiles[@]}"; do bash "$PWD/tests/compile-arduino.sh" "$selected"; done
  exit 0
fi
valid=false
for selected in "${profiles[@]}"; do
  if [[ "$selected" == "$profile" ]]; then valid=true; break; fi
done
if [[ "$valid" != true ]]; then
  printf 'Unknown Arduino build profile: %s\n' "$profile" >&2
  exit 2
fi
repo_root="$PWD"
build_root=$(mktemp -d "${TMPDIR:-/tmp}/ardportal-builds.XXXXXX")
trap 'rm -rf "$build_root"' EXIT
cli="${ARDUINO_CLI:-arduino-cli}"
fqbn=esp32:esp32:esp32
sketch=examples/DynamicPages
properties=()
case "$profile" in
  portal-esp8266) fqbn=esp8266:esp8266:generic ;;
  portal-esp32) ;;
  zigbee-*-ed)
    chip="esp32${profile#zigbee-}"
    chip="${chip%-ed}"
    fqbn="esp32:esp32:${chip}:ZigbeeMode=ed,PartitionScheme=zigbee"
    sketch=examples/ZigbeeClimate
    properties=(--build-property upload.maximum_size=1835008)
    ;;
  matter-*)
    sketch=examples/MatterPortal
    fqbn=esp32:esp32:esp32c6:PartitionScheme=huge_app
    properties=(--build-property upload.maximum_size=3670016)
    case "$profile" in
      matter-c6-wifi) flags=-DARDPORTAL_MATTER_THREAD=0 ;;
      matter-c6-thread) flags=-DARDPORTAL_MATTER_THREAD=1 ;;
      matter-h2-thread)
        fqbn=esp32:esp32:esp32h2:PartitionScheme=huge_app
        flags=-DARDPORTAL_MATTER_THREAD=1 ;;
      matter-c6-full) flags=-DARDPORTAL_ENABLE_CONTROLS=1 ;;
    esac
    properties+=(--build-property "compiler.cpp.extra_flags=$flags")
    ;;
  api-*|no-network-esp32)
    # Generate the same API probes as a full local run, compile only this case.
    "${PYTHON:-python3}" tests/public-api-matrix.py "$build_root/api"
    if [[ "$profile" == no-network-esp32 ]]; then
      sketch="$build_root/api/no_network"
    else
      fqbn=esp8266:esp8266:generic
      sketch="$build_root/api/${profile#api-}"
    fi
    ;;
esac
# Core 3.1.2 elf2bin.py uses a non-raw \s regex; retain other warnings.
esp8266_python_warnings="ignore:invalid escape sequence '\\s':SyntaxWarning${PYTHONWARNINGS:+,$PYTHONWARNINGS}"
build_command=("$cli" compile --fqbn "$fqbn" --library "$repo_root"
  --build-path "$build_root/build")
if [[ ${#properties[@]} -gt 0 ]]; then build_command+=("${properties[@]}"); fi
build_command+=("$sketch")
if [[ "$fqbn" == esp8266:* ]]; then
  env PYTHONWARNINGS="$esp8266_python_warnings" "${build_command[@]}"
else
  "${build_command[@]}"
fi
if [[ "$profile" == no-network-esp32 ]]; then
  "${PYTHON:-python3}" - "$build_root/build/no_network.ino.map" <<'PY'
import re
import sys
from pathlib import Path

link_map = Path(sys.argv[1]).read_text()
assert not re.search(r'^\s+0x[0-9a-fA-F]+\s+esp_wifi_\w+\s*$', link_map, re.MULTILINE), \
    'Network-free ESP32 firmware unexpectedly retains Wi-Fi driver symbols'
print('PASS: network-free ESP32 firmware excludes Wi-Fi driver symbols')
PY
fi
