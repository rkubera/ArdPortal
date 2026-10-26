#!/usr/bin/env python3
"""Exercise the SDK overlay against dual-band, single-band and changed sources."""
import pathlib
import shutil
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
cmake = shutil.which('cmake')
if not cmake:
    raise SystemExit('CMake is required for the Matter Wi-Fi band overlay test')
with tempfile.TemporaryDirectory() as tmp:
    base = pathlib.Path(tmp)
    for name, wifi, dual, changed, success in [
        ('dual', True, True, False, True),
        ('single', True, False, False, True),
        ('thread', False, True, False, True),
        ('changed', True, True, True, False),
    ]:
        project = base / name
        driver = project / 'platform/ESP32'
        driver.mkdir(parents=True)
        cpp = 'uint32_t bands = static_cast<uint32_t>(1UL << chip::to_underlying(WiFiBandEnum::k2g4));'
        header = 'item.wiFiBand = chip::DeviceLayer::NetworkCommissioning::WiFiBand::k2g4;'
        (driver / 'NetworkCommissioningDriver.cpp').write_text('changed' if changed else cpp)
        (driver / 'NetworkCommissioningDriver.h').write_text(header)
        (project / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.20)
project(Bands CXX)
add_library(sdk OBJECT platform/ESP32/NetworkCommissioningDriver.cpp)
set(matter_lib sdk)
set(matter_sources "${{CMAKE_CURRENT_SOURCE_DIR}}/platform/ESP32/NetworkCommissioningDriver.cpp")
set(CONFIG_ENABLE_WIFI_STATION {'TRUE' if wifi else 'FALSE'})
set(CONFIG_SOC_WIFI_SUPPORT_5G {'TRUE' if dual else 'FALSE'})
include("{root}/idf/MatterPortal/MatterWiFiBands.cmake")
''')
        result = subprocess.run([cmake, '-S', str(project), '-B', str(project / 'build')], capture_output=True, text=True)
        assert (result.returncode == 0) == success, result.stdout + result.stderr
        overlay = project / 'build/ardportal-matter-wifi'
        assert overlay.exists() == (wifi and dual and success)
        assert (driver / 'NetworkCommissioningDriver.h').read_text() == header
        if overlay.exists():
            assert 'WiFiBandEnum::k5g' in (overlay / 'NetworkCommissioningDriver.cpp').read_text()
            assert 'ap_record.primary > 14' in (overlay / 'platform/ESP32/NetworkCommissioningDriver.h').read_text()
print('PASS: Matter dual-band overlay, feature guards and changed-SDK rejection')
