#!/usr/bin/env python3
"""Exercise Arduino matrix dispatch without downloading cores or compiling firmware."""
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
script = root / 'tests/compile-arduino.sh'
workflow = (root / '.github/workflows/pr-checks.yml').read_text()
job = workflow.split('  arduino-builds:', 1)[1].split('  matter-sdk:', 1)[0]
profiles = subprocess.check_output(['bash', str(script), '--list'], text=True).splitlines()
entries = re.findall(r'- profile: (\S+)\n\s+name: "([^"]+)"\n\s+core: (\S+)', job)
assert len(profiles) == len(set(profiles)) == 19
assert len(entries) == len({entry[0] for entry in entries}) == 9
assert {entry[0] for entry in entries} <= set(profiles)
assert 'fail-fast: false' in job and "compile-arduino.sh '${{ matrix.profile }}'" in job
assert 'Firmware / Arduino / ${{ matrix.name }}' in job

with tempfile.TemporaryDirectory(prefix='ardportal-ci-dispatch-') as directory:
    path = Path(directory)
    cli = path / 'arduino-cli'
    cli.write_text('''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
args=sys.argv[1:]
with open(os.environ['ARD_TEST_LOG'],'a') as output:output.write(json.dumps(args)+'\\n')
if os.environ.get('ARD_TEST_FAIL'):sys.exit(17)
build=Path(args[args.index('--build-path')+1]);build.mkdir(parents=True,exist_ok=True)
if Path(args[-1]).name=='no_network' and args[args.index('--fqbn')+1]=='esp32:esp32:esp32':
 (build/'no_network.ino.map').write_text('  0x42000000 esp_wifi_init\\n' if os.environ.get('ARD_TEST_WIFI') else '')
''')
    cli.chmod(0o755)
    log = path / 'commands.jsonl'
    env = {**os.environ, 'ARDUINO_CLI': str(cli), 'ARD_TEST_LOG': str(log)}
    for profile in profiles:
        core = 'esp8266:esp8266@3.1.2' if profile == 'portal-esp8266' or profile.startswith('api-') else 'esp32:esp32@3.3.12'
        log.write_text('')
        result = subprocess.run(['bash', str(script), profile], env=env, capture_output=True, text=True)
        assert result.returncode == 0, (profile, result.stderr)
        commands = [json.loads(line) for line in log.read_text().splitlines()]
        assert len(commands) == 1, (profile, commands)
        args = commands[0]
        assert args[0] == 'compile'
        fqbn = args[args.index('--fqbn') + 1]
        assert fqbn.startswith(core.split('@')[0] + ':'), (profile, fqbn)
        if profile.startswith('zigbee-'):
            chip = 'esp32' + profile.split('-')[1]
            assert fqbn == f'esp32:esp32:{chip}:ZigbeeMode=ed,PartitionScheme=zigbee'
            assert args[-1] == 'examples/ZigbeeClimate'
            assert 'upload.maximum_size=1835008' in args
        elif profile.startswith('matter-'):
            assert args[-1] == 'examples/MatterPortal'
            assert 'upload.maximum_size=3670016' in args
            flag = '-DARDPORTAL_ENABLE_CONTROLS=1' if profile.endswith('full') else '-DARDPORTAL_MATTER_THREAD=' + str(int(profile.endswith('thread')))
            assert 'compiler.cpp.extra_flags=' + flag in args
            assert ('esp32h2' in fqbn) == ('-h2-' in profile)
        elif profile.startswith('api-'):
            assert Path(args[-1]).name == profile.removeprefix('api-')
        elif profile == 'no-network-esp32':
            assert Path(args[-1]).name == 'no_network'
            assert 'excludes Wi-Fi driver symbols' in result.stdout
        else:
            assert args[-1] == 'examples/DynamicPages'
    # Keep the existing all-profiles local entry point and fail-closed checks.
    log.write_text('')
    subprocess.run(['bash', str(script)], env=env, check=True, capture_output=True)
    assert len(log.read_text().splitlines()) == len(profiles)
    for profile, extra, expected in [('no-network-esp32', {'ARD_TEST_WIFI':'1'}, 1),
                                     ('portal-esp32', {'ARD_TEST_FAIL':'1'}, 17),
                                     ('unknown', {}, 2)]:
        result = subprocess.run(['bash', str(script), profile], env={**env, **extra}, capture_output=True, text=True)
        assert result.returncode == expected, (profile, result.returncode, result.stderr)
print('PASS: 9 CI Arduino jobs, 19 local profiles, target/core/flag dispatch, all-profiles entry point and linker/compiler failures')
