#!/usr/bin/env python3
"""Regression tests for trusted portal HTML asset extraction."""
import itertools
from html.parser import HTMLParser
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src/tools'))
from generate_portal_assets import PortalSections, portal_parts, compact_html
from portal_identifiers import identifier_maps


class PortalAssetsTest(unittest.TestCase):
    def test_compact_html_preserves_parsed_content_and_inline_code(self):
        class Events(HTMLParser):
            def __init__(self, source):
                super().__init__(convert_charrefs=False)
                self.events = []
                self.feed(source)
                self.close()

            def handle_starttag(self, tag, attrs):
                self.events.append(('start', tag, attrs))

            def handle_endtag(self, tag):
                self.events.append(('end', tag))

            def handle_data(self, data):
                self.events.append(('data', data))

            def handle_comment(self, data):
                self.events.append(('comment', data))

            def handle_entityref(self, name):
                self.events.append(('entity', name))

        cases = [
            '<input id="name" value="" maxlength="32" disabled>',
            '<input value="x"/><input value="x" />',
            '<p class="one two" title="a&amp;b"> A &amp; B </p>',
            '<button onclick=\'console.log(" x=\\"y\\"")\' id="ok">Go</button>',
            '<!-- <b id="comment"> --><script>const s=\'<b id="text">\';</script>',
            '<style>.x:after{content:"a b"}</style><textarea>  x\n y </textarea>',
        ]
        for source in cases:
            with self.subTest(source=source):
                compact = compact_html(source)
                self.assertEqual(Events(source).events, Events(compact).events)
                self.assertEqual(compact_html(compact), compact)
        self.assertEqual(compact_html('<input id="name">'), '<input id=name>')

    def test_console_styles_follow_feature_selection(self):
        classes, _ = identifier_maps((ROOT/'src/PortalPage.h').read_text())
        for enabled, controls in itertools.product((False, True), (None, [], ['text'], ['slider', 'select'], ['light', 'cover'])):
            with self.subTest(console=enabled, controls=controls):
                css, _, _ = portal_parts(ROOT / 'src', console=enabled, controls=controls)
                self.assertEqual('.'+classes['consoleLog']+'{' in css, enabled)
                self.assertEqual('.'+classes['consoleTabs']+'{' in css, enabled)

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
