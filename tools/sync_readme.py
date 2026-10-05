#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Keep the distributable README identical to the project usage guide."""
import argparse
from pathlib import Path

root = Path(__file__).resolve().parents[3]
source = root / 'README.md'
target = root / 'src' / 'ArdPortal' / 'README.md'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check', action='store_true')
args = parser.parse_args()
text = source.read_text(encoding='utf-8')
if args.check:
    if not target.exists() or target.read_text(encoding='utf-8') != text:
        raise SystemExit('Library README is stale; run tools/sync_readme.py')
else:
    target.write_text(text, encoding='utf-8')
