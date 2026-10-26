#!/usr/bin/env python3
"""Verify bounded download retries and checksum enforcement without network access."""
import hashlib
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import MagicMock, patch
from urllib.error import HTTPError, URLError

spec = importlib.util.spec_from_file_location(
    'installer', Path(__file__).with_name('install-arduino-tool.py'))
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class DownloadTest(unittest.TestCase):
    def test_transient_failures_recover(self):
        response = MagicMock()
        response.__enter__.return_value.read.return_value = b'archive'
        failures = [HTTPError('url', 504, 'timeout', {}, None), URLError('reset'), response]
        with patch.object(installer.urllib.request, 'urlopen', side_effect=failures) as fetch, \
                patch.object(installer.time, 'sleep') as sleep:
            self.assertEqual(installer.download('url'), b'archive')
            self.assertEqual(fetch.call_count, 3)
            self.assertEqual([call.args[0] for call in sleep.call_args_list], [2, 4])

    def test_retries_are_bounded(self):
        with patch.object(installer.urllib.request, 'urlopen', side_effect=TimeoutError) as fetch, \
                patch.object(installer.time, 'sleep') as sleep:
            with self.assertRaises(TimeoutError):
                installer.download('url')
            self.assertEqual(fetch.call_count, 5)
            self.assertEqual(sleep.call_count, 4)

    def test_permanent_http_failure_is_immediate(self):
        with patch.object(installer.urllib.request, 'urlopen',
                          side_effect=HTTPError('url', 404, 'missing', {}, None)) as fetch, \
                patch.object(installer.time, 'sleep') as sleep:
            with self.assertRaises(HTTPError):
                installer.download('url')
            self.assertEqual(fetch.call_count, 1)
            sleep.assert_not_called()

    def test_verified_install_and_checksum_rejection(self):
        archive = io.BytesIO()
        with tarfile.open(fileobj=archive, mode='w:gz') as bundle:
            entry = tarfile.TarInfo('arduino-lint')
            entry.size = 4
            bundle.addfile(entry, io.BytesIO(b'tool'))
        payload = archive.getvalue()
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(installer, 'download', return_value=payload), \
                patch.dict(installer.TOOLS, {'arduino-lint': ('test', 'archive',
                               hashlib.sha256(payload).hexdigest())}):
            target = Path(directory) / 'bin'
            installer.install('arduino-lint', target)
            self.assertEqual((target / 'arduino-lint').read_bytes(), b'tool')
            installer.TOOLS['arduino-lint'] = ('test', 'archive', 'wrong')
            with self.assertRaises(SystemExit):
                installer.install('arduino-lint', Path(directory) / 'bad')
            self.assertFalse((Path(directory) / 'bad').exists())


if __name__ == '__main__':
    unittest.main()
