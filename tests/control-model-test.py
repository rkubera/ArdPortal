#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Exercise the full semantic contract and prove disabled names leave the image."""
import json
import os
import subprocess
import tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
TYPES = json.loads((ROOT / 'src/tools/control_types.json').read_text())
CLASSES = json.loads((ROOT / 'src/tools/control_classes.json').read_text())

def compile_run(work, name, body, flags=()):
    source = work / (name + '.cpp')
    target = work / name
    source.write_text('#include <cassert>\n#define ARDPORTAL_DEFINE_CONTROL_DATA\n#include "ControlModel.h"\nuint32_t fakeMillis=0;\nusing V=ArdJSON::JSONVar;\nint main(){\n' + body + '\n}\n')
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Os',
                    '-fsanitize=address,undefined', '-I'+str(ROOT/'tests/host'), '-I'+str(ROOT/'src'),
                    *flags, str(source), '-o', str(target)], check=True)
    subprocess.run([str(target)], check=True)
    return target.read_bytes()

with tempfile.TemporaryDirectory(prefix='ardportal-control-model-') as directory:
    work = Path(directory)
    lines = ['V field = V::object();']
    for control in TYPES:
        lines += ['field = V::object(); field["type"] = ' + json.dumps(control) + '; assert(ArdControl::validClass(field));']
        for name, spec in CLASSES.items():
            lines += ['field["class"] = ' + json.dumps(name) + '; assert(ArdControl::validClass(field) == ' + str(control in spec['types']).lower() + ');']
        if control not in TYPES[:6]:
            lines += ['field.remove("class"); assert(ArdControl::normalize(field)); assert(!field.hasOwnProperty("ha"));']
    lines += [r'''
    field=ArdJSON::JSON.parse(R"({"type":"event","class":"doorbell"})");
    assert(ArdControl::normalize(field));
    assert(field["event_types"][0].asString()=="ring");
    assert(ArdControl::validValue(field,ArdJSON::JSON.parse(R"({"event_type":"ring"})")));
    assert(!ArdControl::validValue(field,ArdJSON::JSON.parse(R"({"event_type":"wrong"})")));
    field=ArdJSON::JSON.parse(R"({"type":"binary_sensor"})");
    assert(ArdControl::normalize(field)); assert(ArdControl::describe(field).has(ArdControl::Measurement));
    field=ArdJSON::JSON.parse(R"({"type":"light","controls":[{"key":"state","type":"switch","on":"ON","off":"OFF","command":""}]})");
    auto model=ArdControl::describe(field); assert(model.has(ArdControl::OnOff) && !model.has(ArdControl::Color) && !model.has(ArdControl::Level));
    field=ArdJSON::JSON.parse(R"({"type":"text","class":"date"})");
    assert(ArdControl::validReading(field,"2024-02-29")); assert(!ArdControl::validReading(field,"2025-02-29"));
    field["class"]="timestamp"; assert(ArdControl::validReading(field,"2026-10-23T12:30:00Z")); assert(!ArdControl::validReading(field,"2026-10-23"));
    field["class"]="uptime"; assert(ArdControl::validReading(field,100)); assert(!ArdControl::validReading(field,-1));
    field=ArdJSON::JSON.parse(R"({"type":"infrared","class":"receiver"})");
    assert(ArdControl::normalize(field)); assert(ArdControl::readonly(field) && !field["controls"].length());
    assert(ArdControl::validValue(field,ArdJSON::JSON.parse(R"({"timings":[9000,-4500],"modulation":38000})")));
    assert(!ArdControl::validValue(field,ArdJSON::JSON.parse(R"({"timings":[0]})")));
    ''']
    compile_run(work, 'all', '\n'.join(lines), ['-DARDPORTAL_ENABLE_HA=0'])
    image = compile_run(work, 'switch', r'''
    V field=ArdJSON::JSON.parse(R"({"type":"switch","class":"outlet"})");
    assert(ArdControl::validClass(field));
    assert(ArdControl::classInfo(ArdControl::Class::Temperature).name==nullptr);
    assert(ArdControl::classInfo(ArdControl::Class::Blind).name==nullptr);
    ''', ['-DARDPORTAL_ENABLE_CONTROLS=0', '-DARDPORTAL_ENABLE_CONTROL_SWITCH=1'])
    for name in ['blind','firmware','carbon_dioxide','transmitter','irradiance','battery_low']:
        assert name.encode()+b'\0' not in image, 'Disabled name retained in binary: '+name
    # ArdJSON interns common state keys such as temperature/humidity independently of controls.
    assert b'name_temperature' not in image and b'name_humidity' not in image
    compile_run(work, 'slider', r'''
    V field=ArdJSON::JSON.parse(R"({"type":"slider","class":"temperature"})");
    assert(ArdControl::validClass(field));
    assert(ArdControl::classInfo(ArdControl::Class::Outlet).name==nullptr);
    ''', ['-DARDPORTAL_ENABLE_CONTROLS=0', '-DARDPORTAL_ENABLE_CONTROL_SLIDER=1'])
print('PASS: all 30 controls, every class/type pair, protocol-free defaults, capabilities, events, calendar/IR validation and excluded binary names')
