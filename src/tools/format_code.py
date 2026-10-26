#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Format or check all library, example and host-test C++ using Arduino IDE 2 style."""
import argparse
import subprocess
from cpp_format import ROOT, formatter


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check formatting without writing files')
    args = parser.parse_args()
    paths = sorted(path for folder in ('src', 'examples', 'tests', 'idf/MatterPortal/main')
                   for path in (ROOT / folder).rglob('*')
                   if path.suffix in {'.h', '.hpp', '.c', '.cpp', '.ino'})
    mode = ['--dry-run', '--Werror', '--ferror-limit=1'] if args.check else ['-i']
    subprocess.run([formatter(), '--style=file:' + str(ROOT / '.clang-format'),
                    *mode, *map(str, paths)], check=True)
    print(('Checked' if args.check else 'Formatted') + f' {len(paths)} C++/Arduino files')


if __name__ == '__main__':
    main()
