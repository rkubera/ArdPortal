#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Validate resolved SDK configuration and live network stacks in an IDF build."""
import argparse
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=Path)
parser.add_argument('--profile', required=True, choices=('wifi', 'thread', 'thread-wifi'))
parser.add_argument('--credentials', default='development', choices=('development', 'production'))
parser.add_argument('--power', default='off', choices=('off', 'connected', 'icd'))
parser.add_argument('--thread-role', default='mtd', choices=('mtd', 'router'))
args = parser.parse_args()
cache = (args.build / 'CMakeCache.txt').read_text()
match = re.search(r'^SDKCONFIG:[^=]+=(.+)$', cache, re.MULTILINE)
config_path = Path(match[1]) if match else args.build / 'sdkconfig'
config = config_path.read_text()

def enabled(name):
    return f'CONFIG_{name}=y' in config.splitlines()

assert enabled('ARDPORTAL_MATTER'), 'Matter adapter disabled'
profile_option = {'wifi': 'WIFI', 'thread': 'THREAD_ONLY', 'thread-wifi': 'THREAD_WIFI'}[args.profile]
assert enabled('ARDPORTAL_MATTER_' + profile_option), 'Wrong ArdPortal SDK profile'
assert enabled('ENABLE_CHIPOBLE') and enabled('BT_ENABLED'), 'BLE commissioning must remain enabled'
assert not enabled('COMPILER_CXX_EXCEPTIONS'), 'SDK profiles require C++ exceptions disabled'
for option in ('ESP_WIFI_IRAM_OPT', 'ESP_WIFI_EXTRA_IRAM_OPT', 'ESP_WIFI_RX_IRAM_OPT', 'ESP_WIFI_SLP_IRAM_OPT'):
    assert not enabled(option), 'Keep SRAM available for Wi-Fi authentication, BLE and HTTP'
assert 'CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1' in config.splitlines()
assert enabled('BT_NIMBLE_ROLE_PERIPHERAL'), 'BLE peripheral commissioning required'
assert not enabled('BT_NIMBLE_ROLE_CENTRAL') and not enabled('BT_NIMBLE_ROLE_OBSERVER')
for option, count in {'MSYS_1_BLOCK_COUNT': 12, 'MSYS_2_BLOCK_COUNT': 8,
                      'TRANSPORT_ACL_FROM_LL_COUNT': 8, 'TRANSPORT_EVT_COUNT': 12,
                      'TRANSPORT_EVT_DISCARD_COUNT': 4}.items():
    assert f'CONFIG_BT_NIMBLE_{option}={count}' in config.splitlines(), 'Stale NimBLE buffer configuration'
production = args.credentials == 'production'
assert enabled('ARDPORTAL_MATTER_PRODUCTION') == production
assert enabled('ARDPORTAL_MATTER_DEVELOPMENT') != production
assert enabled('ENABLE_ESP32_FACTORY_DATA_PROVIDER') == production
assert enabled('ENABLE_ESP32_DEVICE_INSTANCE_INFO_PROVIDER') == production
assert enabled('ENABLE_TEST_SETUP_PARAMS') != production
for provider in ('DAC', 'COMMISSIONABLE_DATA', 'DEVICE_INSTANCE_INFO'):
    factory = 'FACTORY_PARTITION_DAC_PROVIDER' if provider == 'DAC' else 'FACTORY_' + provider + '_PROVIDER'
    assert enabled(factory) == production
    assert enabled('EXAMPLE_' + provider + '_PROVIDER') != production
partition = 'partitions.production.csv' if production else 'partitions.csv'
assert f'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="{partition}"' in config.splitlines()
label = 'fctry' if production else 'nvs'
assert f'CONFIG_CHIP_FACTORY_NAMESPACE_PARTITION_LABEL="{label}"' in config.splitlines()
if args.power != 'off':
    assert enabled('PM_ENABLE') and enabled('FREERTOS_USE_TICKLESS_IDLE')
assert enabled('ENABLE_ICD_SERVER') == (args.power == 'icd')
if args.power == 'icd':
    assert args.profile != 'wifi' and enabled('OPENTHREAD_MTD') and enabled('IEEE802154_SLEEP_ENABLE')
    assert not enabled('ENABLE_ICD_LIT'), 'This profile implements Short Idle Time ICD'
thread = args.profile != 'wifi'
router = args.thread_role == 'router'
assert not router or (thread and args.power != 'icd'), 'Router requires Thread without ICD'
if thread:
    assert enabled('OPENTHREAD_FTD') == router
    assert enabled('OPENTHREAD_MTD') != router
if router:
    assert not enabled('IEEE802154_SLEEP_ENABLE'), 'Router must keep the Thread radio listening'
wifi = args.profile != 'thread'
assert enabled('ENABLE_MATTER_OVER_THREAD') == thread
assert enabled('OPENTHREAD_ENABLED') == thread
assert enabled('ENABLE_WIFI_STATION') == (args.profile == 'wifi')
if args.profile == 'thread-wifi':
    assert enabled('ESP_COEX_SW_COEXIST_ENABLE'), 'Mixed radios require SDK coexistence'
    assert 'CONFIG_THREAD_NETWORK_ENDPOINT_ID=0' in config.splitlines(), 'Thread commissioning belongs on the root endpoint'
    assert enabled('CHIP_SYSTEM_CONFIG_POOL_USE_HEAP'), 'Mixed profile requires on-demand Matter pools'
    for key, value in {'OPENTHREAD_NUM_MESSAGE_BUFFERS': 64 if router else 32, 'EVENT_LOGGING_CRIT_BUFFER_SIZE': 4096, 'EVENT_LOGGING_DEBUG_BUFFER_SIZE': 1024}.items():
        assert f'CONFIG_{key}={value}' in config.splitlines(), 'Stale mixed-profile memory buffers: ' + key
if wifi:
    assert enabled('ESP_WIFI_ENABLED')
commands = json.loads((args.build / 'compile_commands.json').read_text())
cpp_commands = [c for c in commands if Path(c['file']).suffix in ('.cpp', '.cc', '.cxx')]
dual_band_matter = args.profile == 'wifi' and enabled('SOC_WIFI_SUPPORT_5G')
band_overlays = [c for c in commands if '/ardportal-matter-wifi/NetworkCommissioningDriver.cpp' in c['file']]
assert len(band_overlays) == int(dual_band_matter), 'Unexpected Matter dual-band commissioning driver'
if dual_band_matter:
    band_root = Path(band_overlays[0]['file']).parent
    assert 'WiFiBandEnum::k5g' in (band_root / 'NetworkCommissioningDriver.cpp').read_text()
    assert 'ap_record.primary > 14' in (band_root / 'platform/ESP32/NetworkCommissioningDriver.h').read_text()
policy_sources = [c['file'] for c in commands if 'MatterMemoryPolicy.h' in c.get('command', ' '.join(c.get('arguments', [])))]
expect_policy = args.profile == 'thread-wifi' and enabled('ESP_SYSTEM_ALLOW_RTC_FAST_MEM_AS_HEAP') and not enabled('SPIRAM')
assert len(policy_sources) == int(expect_policy), 'Memory policy must affect only the SDK model allocator'
if expect_policy:
    assert policy_sources[0].endswith('/MatterMemWrap.cpp'), policy_sources
assert cpp_commands, 'No C++ compilation commands found'
for entry in cpp_commands:
    flags = entry.get('command', ' '.join(entry.get('arguments', []))).split()
    assert '-fno-exceptions' in flags and '-fexceptions' not in flags, entry['file']
main = next(c for c in commands if c['file'].endswith('/MatterPortal/main/main.cpp'))
command = main.get('command', ' '.join(main.get('arguments', [])))
assert '-DARDPORTAL_ENABLE_MATTER=1' in command
assert ('-DARDPORTAL_MATTER_PRODUCTION=1' in command) == production
assert f'-DARDPORTAL_MATTER_THREAD={int(thread)}' in command
if args.profile == 'thread-wifi':
    assert 'ARDUINO_MATTER_NETWORK_THREAD' not in command, 'Arduino C5 Thread override disables the portal Wi-Fi SDK'
if not wifi:
    assert '-DARDPORTAL_ENABLE_WIFI=0' in command

# Ignore discarded sections and the cross-reference table: they mention unused libraries too.
link_map = (args.build / 'ArdPortalMatter.map').read_text()
link_map = link_map.split('Linker script and memory map', 1)[1].split('Cross Reference Table', 1)[0]
coex_sections = re.findall(r'\.text\.esp_coex_wifi_i154_enable\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)', link_map)
coex_live = any(int(address, 16) >= 0x40000000 and int(size, 16) for address, size in coex_sections)
router_sections = re.findall(r'\.text\.otThreadSetRouterEligible\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)', link_map)
router_live = any(int(address, 16) >= 0x40000000 and int(size, 16) for address, size in router_sections)
assert router_live == router, 'Unexpected router-eligibility code in linked firmware'
assert coex_live == (args.profile == 'thread-wifi'), 'Unexpected Wi-Fi/802.15.4 coexistence initialization in linked firmware'
assert ('ArdMatterThreadTransport' in link_map) == (args.profile == 'thread-wifi'), 'Mixed profile must bind Matter IPv6 UDP to Thread'
live = []
for line in link_map.splitlines():
    match = re.match(r'\s+(?:\.[^\s]+\s+)?0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)', line)
    if match and int(match[1], 16) >= 0x40000000 and int(match[2], 16):
        live.append(match[3])
assert any('ICDManager.cpp.obj' in name for name in live) == (args.power == 'icd'), 'Unexpected live ICD manager'
if production:
    assert any('ESP32FactoryDataProvider.cpp.obj' in name for name in live), 'Production factory provider missing from linked firmware'
    assert 'GetExampleDACProvider' not in link_map, 'Example attestation provider retained in production firmware'
assert any('openthread' in name.lower() and '.a(' in name for name in live) == thread, 'Unexpected live OpenThread code'
if thread:
    assert not any('NetworkCommissioningDriver.cpp.obj' in name for name in live), 'Matter Wi-Fi commissioning retained in a Thread profile'
    assert not any('ESP32DnssdImpl.cpp.obj' in name for name in live), 'Matter Wi-Fi discovery retained in a Thread profile'
wifi_driver = any(re.search(r'lib(?:net80211|pp|esp_wifi)\.a\(', name) for name in live)
assert wifi_driver == wifi, 'Unexpected live Wi-Fi driver'
assert any(re.search(r'lib(?:bt|ble_app|btdm_app)\.a\(', name) for name in live), 'BLE stack missing'
print(f'PASS: {args.profile}/{args.credentials}/{args.power}/{args.thread_role}: correct SDK transport, adapter defines and live linker network stacks; BLE retained')
