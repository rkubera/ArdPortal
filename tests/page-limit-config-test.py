#!/usr/bin/env python3
"""Check page-limit bounds and cross-translation-unit layout protection."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get('CXX', 'c++')
flags = [compiler, '-std=c++17', '-I' + str(root / 'tests/memory-host'), '-I' + str(root / 'src')]
for limit in (None, '1', '16', '32', '1024', '0', '-1', '1025', '1.5'):
    options = [] if limit is None else ['-DARDPORTAL_APP_CONFIG_MAX_PAGES=' + limit]
    expected = '16' if limit is None else limit
    source = '#include "AppConfigFields.h"\nstatic_assert(ArdAppConfigMaxPages == ' + expected + ', "limit");\n'
    result = subprocess.run(flags + options + ['-fsyntax-only', '-x', 'c++', '-'], input=source, text=True, capture_output=True)
    valid = limit in (None, '1', '16', '32', '1024')
    assert (result.returncode == 0) == valid, (limit, result.stderr)

with tempfile.TemporaryDirectory(prefix='ardportal-page-limit-') as directory:
    work = Path(directory)
    (work / 'owner.cpp').write_text('#include "ArdPortalFeatures.h"\nvoid check(ArdPortalBuildTag<0>) {}\n')
    (work / 'caller.cpp').write_text('#include "ArdPortalFeatures.h"\nvoid check(ArdPortalBuildTag<0>);\nint main(){check({});}\n')
    for name, limit in (('owner', 16), ('caller', 16), ('mismatch', 32)):
        source = 'caller' if name == 'mismatch' else name
        subprocess.run(flags + ['-DARDPORTAL_APP_CONFIG_MAX_PAGES=' + str(limit), '-c', str(work / (source + '.cpp')), '-o', str(work / (name + '.o'))], check=True)
    for name, valid in (('caller', True), ('mismatch', False)):
        result = subprocess.run([compiler, str(work / 'owner.o'), str(work / (name + '.o')), '-o', str(work / 'test')], capture_output=True, text=True)
        assert (result.returncode == 0) == valid, result.stderr
print('PASS: default/custom page limits, invalid bounds and mismatched layout rejection')
