#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Generate compact validation schemas without Home Assistant discovery metadata."""
import argparse
import json
import re
from pathlib import Path
p = Path(__file__).resolve().parents[1] / 'HaEntityData.h'
s = p.read_text()
s = re.sub(r'// BEGIN GENERATED STATE SCHEMAS.*?// END GENERATED STATE SCHEMAS', '// BEGIN GENERATED STATE SCHEMAS\n// END GENERATED STATE SCHEMAS', s, flags=re.S)
s = re.sub(r'static const char ARD_HA_STATE_\d+\[\] PROGMEM = R"STATE\(.*?\)STATE";\n', '', s)
lines = []
for index, raw in re.findall(r'ARD_HA_SPEC_(\d+)\[\] PROGMEM = R"SPEC\((.*?)\)SPEC";', s):
    value = json.loads(raw)
    value['ha'] = {k: v for k, v in value.get('ha', {}).items() if k == 'event_types'}
    name = re.search(r'ARD_HA_NAME_' + index + r'\[\] PROGMEM = "(.*?)";', s)[1]
    lines.append('#if ARDPORTAL_ENABLE_CONTROL_' + name.upper())
    lines.append(f'static const char ARD_HA_STATE_{index}[] PROGMEM = R"STATE({json.dumps(value, separators=(",", ":"))})STATE";')
    lines.append('#endif')
s = s.replace('// BEGIN GENERATED STATE SCHEMAS\n// END GENERATED STATE SCHEMAS', '// BEGIN GENERATED STATE SCHEMAS\n' + '\n'.join(lines) + '\n// END GENERATED STATE SCHEMAS')
s = re.sub(r'\{ARD_HA_NAME_(\d+),ARD_HA_SPEC_\1(?:,ARD_HA_STATE_\1)?\}', r'{ARD_HA_NAME_\1,ARD_HA_SPEC_\1,ARD_HA_STATE_\1}', s)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
if parser.parse_args().check:
    if p.read_text() != s:
        raise SystemExit('Compact state schemas are stale; run tools/generate_state_schemas.py')
else:
    p.write_text(s)
