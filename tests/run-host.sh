#!/usr/bin/env bash
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
set -euo pipefail
cd "$(dirname "$0")/.."

failed=0
run_test() {
  if ! "$@"; then
    printf 'FAIL: %s\n' "$*" >&2
    failed=1
  fi
}

run_test bash tests/run-config-memory.sh
run_test bash tests/run-topic-templates.sh
run_test bash tests/run-zigbee.sh

for test_name in \
  control-model ap-band-start factory-reset-callback flash-text http-memory mqtt-api mqtt-state-topic mqtt-packet mqtt-serial portal-assets \
  network-api network-features power-policy power-periodic power-wake-cause power-integration power-platform power-events page-stream runtime-state \
  zigbee-power zigbee-composite zigbee-persistence; do
  run_test "${PYTHON:-python3}" "tests/${test_name}-test.py"
done
for test_name in ap-band-ui climate-range-ui wifi-console-ui zigbee-ui; do
  run_test "${NODE:-node}" "tests/${test_name}-test.js"
done

exit "$failed"
