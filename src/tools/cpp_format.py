# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Share the Arduino IDE 2 formatter between source maintenance and generators."""
import functools
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


@functools.lru_cache(maxsize=1)
def formatter():
    """Find clang-format 14, the version bundled with Arduino IDE 2."""
    configured = os.environ.get('CLANG_FORMAT')
    candidates = [shutil.which(configured) or configured if configured else None, shutil.which('clang-format-14'),
                  shutil.which('clang-format'),
                  '/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/clang-format']
    for candidate in filter(None, candidates):
        if not Path(candidate).is_file():
            continue
        version = subprocess.check_output([candidate, '--version'], text=True)
        if 'clang-format version 14.' in version:
            return candidate
    raise RuntimeError('clang-format 14 is required. Install it or set CLANG_FORMAT to its executable.')


def format_cpp(source, filename):
    """Format generated C++ without altering embedded raw-string resources."""
    return subprocess.check_output(
        [formatter(), '--style=file:' + str(ROOT / '.clang-format'),
         '--assume-filename=' + str(ROOT / 'src' / filename)],
        input=source, text=True)
