#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Install pinned Linux x86-64 Arduino tools for CI after SHA-256 verification."""
import argparse
import hashlib
import io
from pathlib import Path, PurePosixPath
import tarfile
import urllib.request

TOOLS = {
    'arduino-cli': (
        'v1.5.1', 'arduino-cli_1.5.1_Linux_64bit.tar.gz',
        '28a8e119c498a25607821c36cb2dc49e8463941b261a0d99091baa7bc692dd2b',
    ),
    'arduino-lint': (
        '1.3.0', 'arduino-lint_1.3.0_Linux_64bit.tar.gz',
        '181671ca174988f2601e1cdf2b40a552682db8eaa047de33117e67e70338b63e',
    ),
}


def install(tool, directory):
    """Download an official release and extract only its verified executable."""
    tag, archive, checksum = TOOLS[tool]
    url = f'https://github.com/arduino/{tool}/releases/download/{tag}/{archive}'
    request = urllib.request.Request(url, headers={'User-Agent': 'ArdPortal-CI'})
    with urllib.request.urlopen(request, timeout=120) as response:
        payload = response.read()
    if hashlib.sha256(payload).hexdigest() != checksum:
        raise SystemExit(f'{tool}: unexpected archive SHA-256')
    with tarfile.open(fileobj=io.BytesIO(payload), mode='r:gz') as bundle:
        entry = next(member for member in bundle.getmembers()
                     if member.isfile() and PurePosixPath(member.name).name == tool)
        data = bundle.extractfile(entry).read()
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / tool
    target.write_bytes(data)
    target.chmod(0o755)
    print(f'Installed {tool} {tag} at {target}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tool', choices=TOOLS)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    install(args.tool, args.directory)
