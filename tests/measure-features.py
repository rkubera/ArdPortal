#!/usr/bin/env python3
"""Compile isolated feature profiles; retain measurements, sketches and build logs.

Run each platform in a separate process. Build directories are reused per platform.
Flash means the application .bin size; RAM means static data reported by the core.
"""
import argparse
import json
import re
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONTROLS = re.findall(r"#ifndef ARDPORTAL_ENABLE_CONTROL_(\w+)", (ROOT / "src/ArdPortalFeatures.h").read_text())
BASE = dict(WIFI=0, WWW=0, OTA=0, MQTT=0, MQTT_TLS=0, HA=0, CONSOLE=0,
            CONSOLE_MESSAGES=0, DYNAMIC_PAGES=0, DEPENDENCIES=0, CONTROLS=0,
            TEXT_TYPES=0, ZIGBEE=0, ZIGBEE_CONVERTER=0, ZHA_QUIRK=0)


def profiles():
    result = []
    def add(name, parent=None, **flags):
        inherited = BASE if parent is None else next(p['flags'] for p in result if p['name'] == parent)
        result.append(dict(name=name, parent=parent, flags=dict(inherited, **flags)))
    add('minimum')
    add('wifi', 'minimum', WIFI=1)
    add('www', 'wifi', WWW=1)
    add('ota', 'www', OTA=1)
    add('mqtt', 'www', MQTT=1)
    add('mqtt_tls', 'mqtt', MQTT_TLS=1)
    add('console', 'mqtt', CONSOLE=1)
    add('messages', 'console', CONSOLE_MESSAGES=1)
    add('mqtt_debug', 'mqtt', **{'@ARDPORTAL_MQTT_DEBUG_SERIAL': 1})
    add('dynamic', 'mqtt', DYNAMIC_PAGES=1)
    add('dependencies', 'dynamic', DEPENDENCIES=1)
    add('ha', 'dynamic', HA=1)
    add('ha_dependencies', 'ha', DEPENDENCIES=1)
    for control in CONTROLS:
        add('control_' + control.lower(), 'ha', **{'CONTROL_' + control: 1})
    add('text_types', 'control_text', TEXT_TYPES=1)
    add('controls', 'ha', CONTROLS=1)
    add('default', 'controls', OTA=1, MQTT_TLS=1, CONSOLE=1,
        DEPENDENCIES=1, TEXT_TYPES=1)
    add('full', 'controls', OTA=1, MQTT_TLS=1, CONSOLE=1, CONSOLE_MESSAGES=1,
        DEPENDENCIES=1, TEXT_TYPES=1)
    add('definition_limit', 'dynamic', **{'@ARDPORTAL_APP_CONFIG_MAX_DEFINITION_BYTES': 131072})
    for suffix in ['HEAP', 'DMA', 'BLOCK']:
        add('reserve_' + suffix.lower(), 'www', **{'@ARDPORTAL_NETWORK_' + suffix + '_RESERVE': 0})
    return result


def zigbee_profiles():
    result = []
    def add(name, parent=None, **flags):
        inherited = BASE if parent is None else next(p['flags'] for p in result if p['name'] == parent)
        result.append(dict(name=name, parent=parent, flags=dict(inherited, **flags)))
    add('c6_minimum')
    add('zigbee', 'c6_minimum', ZIGBEE=1)
    add('zigbee_router', 'zigbee', **{'@ARDPORTAL_ZIGBEE_ROUTER': 1})
    add('zigbee_debug', 'zigbee', **{'@ARDPORTAL_ZIGBEE_DEBUG_SERIAL': 1})
    add('zigbee_controls', 'zigbee', DYNAMIC_PAGES=1, CONTROLS=1)
    add('zigbee_ota', 'zigbee_controls', OTA=1)
    add('zigbee_converter', 'zigbee_controls', ZIGBEE_CONVERTER=1)
    add('zha_quirk', 'zigbee_controls', ZHA_QUIRK=1)
    for suffix, value in [('MANUFACTURER', 4660), ('IMAGE_TYPE', 2), ('FILE_VERSION', 2)]:
        add('zigbee_identity_' + suffix.lower(), 'zigbee_ota',
            **{'@ARDPORTAL_ZIGBEE_OTA_' + suffix: value})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', default='arduino-cli')
    parser.add_argument('--platform', choices=['esp8266', 'esp32', 'esp32c6'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=3)
    parser.add_argument('--work-root', type=Path, help='Reuse build/sketch directories across measurement passes')
    parser.add_argument('--library-root', type=Path, default=ROOT, help='Library snapshot to compile')
    parser.add_argument('--application-only', action='store_true', help='ESP32: omit bootloader/merged-image generation; application compile/link/image stays unchanged')
    parser.add_argument('--only', nargs='*')
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    covered = {key[1:] if key.startswith('@') else 'ARDPORTAL_ENABLE_' + key
               for profile in profiles() + zigbee_profiles() for key in profile['flags']}
    expected = set()
    for header in ['ArdPortalFeatures.h', 'HeapMemory.h', 'AppConfigFields.h']:
        expected.update(re.findall(r'^#ifndef (ARDPORTAL_\w+)', (ROOT / 'src' / header).read_text(), re.MULTILINE))
    if expected - covered:
        parser.error('Missing measurement profiles for: ' + ', '.join(sorted(expected - covered)))
    output = args.output.resolve() / args.platform
    output.mkdir(parents=True, exist_ok=True)
    work = args.work_root.resolve() / args.platform if args.work_root else output
    sketch = work / 'FeatureProbe'
    sketch.mkdir(parents=True, exist_ok=True)
    build = work / 'build'
    cases = zigbee_profiles() if args.platform == 'esp32c6' else profiles()
    (output / 'profiles.json').write_text(json.dumps(cases, indent=2) + '\n')
    for case in cases:
        name = case['name']
        if args.only and name not in args.only:
            continue
        record = output / (name + '.json')
        if args.resume and record.exists() and json.loads(record.read_text()).get('ok'):
            continue
        source = ''.join('#define ' + (key[1:] if key.startswith('@') else 'ARDPORTAL_ENABLE_' + key) + ' ' + str(value) + '\n' for key, value in case['flags'].items())
        source += '''#include <ArdPortal.h>
ArdPortal portal;
void setup() {
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
  portal.startAppConfigPageRegistration(F("{\\"id\\":\\"probe\\",\\"name\\":\\"Probe\\",\\"fields\\":[]}"));
#endif
#if ARDPORTAL_ZIGBEE_CONVERTER_SUPPORT
  portal.startZigbee2MqttConverterExport([](const char*, size_t) { return true; });
#endif
#if ARDPORTAL_ZHA_QUIRK_SUPPORT
  portal.startZhaQuirkExport([](const char*, size_t) { return true; });
#endif
  portal.begin();
}
void loop() { portal.loop(); }
'''
        (sketch / 'FeatureProbe.ino').write_text(source)
        (output / (name + '.ino')).write_text(source)
        fqbn = 'esp8266:esp8266:generic' if args.platform == 'esp8266' else 'esp32:esp32:esp32'
        extra = []
        if args.platform == 'esp32c6':
            role = 'zczr' if case['flags'].get('@ARDPORTAL_ZIGBEE_ROUTER') else 'ed'
            fqbn = 'esp32:esp32:esp32c6:ZigbeeMode=' + role + ',PartitionScheme=huge_app'
            extra = ['--build-property', 'upload.maximum_size=3145728']
        if args.application_only and args.platform != 'esp8266':
            extra += ['--build-property', 'recipe.hooks.prebuild.4.pattern=',
                      '--build-property', 'recipe.hooks.objcopy.postobjcopy.3.pattern=']
        command = [args.cli, 'compile', '--fqbn', fqbn, '--jobs', str(args.jobs),
                   '--library', str(args.library_root.resolve()), '--build-path', str(build), *extra, str(sketch)]
        started = time.monotonic()
        proc = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        log = proc.stdout
        (output / (name + '.log')).write_text(log)
        metrics = dict(case, platform=args.platform, fqbn=fqbn, ok=proc.returncode == 0,
                       seconds=round(time.monotonic() - started, 1), command=command)
        if proc.returncode == 0:
            metrics['flash_bin'] = (build / 'FeatureProbe.ino.bin').stat().st_size
            if args.platform == 'esp8266':
                metrics['ram_static'] = int(re.search(r'used (\d+) / 80192', log)[1])
                metrics['iram_used'] = int(re.search(r'used (\d+) / 65536', log)[1])
                metrics['flash_irom'] = int(re.search(r'used (\d+) / 1048576', log)[1])
            else:
                metrics['ram_static'] = int(re.search(r'Global variables use (\d+) bytes', log)[1])
                metrics['flash_linked'] = int(re.search(r'Sketch uses (\d+) bytes', log)[1])
            (output / (name + '.map')).write_text((build / 'FeatureProbe.ino.map').read_text())
        record.write_text(json.dumps(metrics, indent=2) + '\n')
        print(json.dumps({k: metrics[k] for k in ['platform', 'name', 'ok', 'seconds', 'flash_bin', 'ram_static'] if k in metrics}), flush=True)
        if not metrics['ok']:
            print(log[-5000:], flush=True)


if __name__ == '__main__':
    main()
