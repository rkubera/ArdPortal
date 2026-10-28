#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Exercise the full semantic contract and prove disabled names leave the image."""
import json
import os
import subprocess
import tempfile
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src/tools'))
from control_protocols import runtime_model
TYPES = json.loads((ROOT / 'src/tools/control_types.json').read_text())
CLASSES = json.loads((ROOT / 'src/tools/control_classes.json').read_text())
MODELS = json.loads((ROOT / 'src/tools/control_models.json').read_text())
HA_PRESETS = json.loads((ROOT / 'tests/fixtures/ha-control-presets.json').read_text())

def compile_run(work, name, body, flags=()):
    source = work / (name + '.cpp')
    target = work / name
    source.write_text('#include <cassert>\n#define ARDPORTAL_DEFINE_CONTROL_DATA\n#include "ControlModel.h"\n#include "HaEntities.h"\nuint32_t fakeMillis=0;\nusing V=ArdJSON::JSONVar;\nint main(){\n' + body + '\n}\n')
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Os',
                    '-fsanitize=address,undefined', '-I'+str(ROOT/'tests/host'), '-I'+str(ROOT/'src'),
                    *flags, str(source), str(ROOT/'src/JsonStream.cpp'), str(ROOT/'src/JsonDocumentStream.cpp'), '-o', str(target)], check=True)
    subprocess.run([str(target)], check=True)
    return target.read_bytes()

with tempfile.TemporaryDirectory(prefix='ardportal-control-model-') as directory:
    work = Path(directory)
    lines = [r'''
    V field = V::object();
    assert(ArdControl::string(V(""), 0));
    assert(ArdControl::string(V("abc"), 3));
    assert(!ArdControl::string(V("abcd"), 3));
    assert(!ArdControl::string(V(42), 128));
    String embedded="a"; embedded+=char(0); embedded+='b';
    assert(!ArdControl::string(V(embedded), 128));
    ''']
    for control in TYPES:
        lines += ['field = V::object(); field["type"] = ' + json.dumps(control) + '; assert(ArdControl::validClass(field));']
        for name, spec in CLASSES.items():
            lines += ['field["class"] = ' + json.dumps(name) + '; assert(ArdControl::validClass(field) == ' + str(control in spec['types']).lower() + ');']
            if control in spec['types']:
                expected = spec['ha']
                if control in ('device_trigger', 'infrared') or (control == 'switch' and name not in ('switch','outlet')):
                    expected = ''
                elif control == 'binary_sensor' and name == 'light':
                    expected = 'light'
                lines += ['{auto id=ArdControl::classOf(field); auto info=ArdControl::classInfo(id); assert(info.name && String(FPSTR(info.name))==' + json.dumps(name) + '); assert(String(FPSTR(info.unit))==' + json.dumps(spec['unit'],ensure_ascii=False) + '); assert(info.numeric==' + str(spec['numeric']).lower() + ' && info.measurement==' + str(spec['measurement']).lower() + '); auto ha=ArdHa::className(ArdControl::describe(field)); ' + ('assert(ha && String(FPSTR(ha))=='+json.dumps(expected)+');' if expected else 'assert(ha==nullptr);') + '}']

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
    for name, spec in MODELS.items():
        expected = json.dumps(runtime_model(spec), separators=(',', ':'), ensure_ascii=False)
        lines += ['assert(ArdJSON::JSON.stringify(ArdControl::descriptor(String('+json.dumps(name)+'))) == String(R"EXPECTED('+expected+')EXPECTED"));']
    compile_run(work, 'all', '\n'.join(lines), ['-DARDPORTAL_ENABLE_HA=0'])
    ha_lines = []
    for name, spec in MODELS.items():
        expected = json.dumps(HA_PRESETS[name], separators=(',', ':'), ensure_ascii=False)
        ha_lines += ['assert(ArdJSON::JSON.stringify(ArdHa::defaults(String('+json.dumps(name)+'))) == String(R"EXPECTED('+expected+')EXPECTED"));']
    compile_run(work, 'ha-presets', '\n'.join(ha_lines))
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
    image = compile_run(work, 'text-no-classes', r'''
    V field=ArdJSON::JSON.parse(R"({"type":"text"})");
    assert(ArdControl::validReading(field,"value"));
    assert(ArdControl::validReading(field,42));
    assert(!ArdControl::validReading(field,V::object()));
    assert(ArdControl::classInfo(ArdControl::Class::Temperature).name==nullptr);
    field["class"]="temperature";
    assert(!ArdControl::validReading(field,42));
    ''', ['-DARDPORTAL_ENABLE_CONTROLS=0', '-DARDPORTAL_ENABLE_CONTROL_TEXT=1', '-DARDPORTAL_ENABLE_CLASSES=0'])
    for symbol in (b'_ZN10ArdControl4date', b'_ZN10ArdControl8datetime', b'name_temperature'):
        assert symbol not in image, 'Disabled class implementation retained: '+str(symbol)
print('PASS: all 30 controls, every class/type pair, protocol-free defaults, capabilities, events, calendar/IR validation and excluded binary names')
