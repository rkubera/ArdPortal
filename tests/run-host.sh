#!/usr/bin/env bash
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
set -euo pipefail
cd "$(dirname "$0")/.."

bash tests/run-config-memory.sh
bash tests/run-topic-templates.sh
bash tests/run-zigbee.sh

for test_name in \
  ap-band-start factory-reset-callback http-memory mqtt-api mqtt-serial \
  network-api network-features page-stream runtime-state \
  zigbee-composite zigbee-persistence; do
  "${PYTHON:-python3}" "tests/${test_name}-test.py"
done
for test_name in ap-band-ui climate-range-ui wifi-console-ui zigbee-ui; do
  "${NODE:-node}" "tests/${test_name}-test.js"
done
