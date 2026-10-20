#!/usr/bin/env python3
"""Regression tests for trusted portal HTML asset extraction."""
import itertools
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src/tools'))
from generate_portal_assets import PortalSections, portal_parts


class PortalAssetsTest(unittest.TestCase):
    def test_preserves_raw_content_and_ignores_comments(self):
        css = '\n.x::before {content: "&amp;";}\n'
        js = '\nconst text = "<script> &amp; >";\n'
        source = '<!-- <script>ignored</script> -->\n<STYLE media="screen">' + css + '</StYlE>\n<ScRiPt type="text/javascript">' + js + '</SCRIPT>'
        sections = PortalSections(source)
        self.assertEqual(sections.content('style'), css)
        self.assertEqual(sections.content('script'), js)
        for tag in ('style', 'script'):
            start, _, _, end = sections.sections[tag]
            self.assertTrue(source[start:end].lower().startswith('<' + tag))

    def test_rejects_missing_unclosed_duplicate_and_external_assets(self):
        for source in (
            '<style>x</style>',
            '<style>x</style><script>x',
            '<style>x</style><script>x</script><script>y</script>',
            '<style>x</style><script src="remote.js"></script>',
        ):
            with self.subTest(source=source), self.assertRaises(ValueError):
                PortalSections(source)

    def test_real_feature_variants_accept_case_and_attributes(self):
        source = (ROOT / 'src/PortalPage.h').read_text()
        modified = source.replace('<style>', '<STYLE media="all">').replace('</style>', '</STYLE>').replace('<script>', '<ScRiPt type="text/javascript">').replace('</script></html>', '</sCrIpT></html>')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'PortalPage.h').write_text(modified)
            for flags in itertools.product((False, True), repeat=5):
                options = dict(zip(('ota', 'mqtt', 'console', 'tls', 'messages'), flags))
                with self.subTest(options=options):
                    self.assertEqual(portal_parts(ROOT / 'src', **options), portal_parts(root, **options))


if __name__ == '__main__':
    unittest.main()
