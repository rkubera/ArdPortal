# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Compact the trusted portal's DOM identifiers without renaming protocol data."""
import functools
import json
import re
from collections import Counter
from html.parser import HTMLParser

SAFE_NAME = r'[A-Za-z_][-A-Za-z0-9_]*'
QUOTED = r'''(?:'(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*")'''


def short_name(index):
    first = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
    second = first + '0123456789'
    if not 0 <= index < len(first) * len(second):
        raise ValueError('Two-character DOM alias capacity exceeded')
    return first[index // len(second)] + second[index % len(second)]


@functools.lru_cache(maxsize=4)
def identifier_maps(source):
    body = source.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    css = re.search(r'<style(?:\s[^>]*)?>(.*?)</style\s*>', body, re.I|re.S)[1]
    selectors = ' '.join(re.findall(r'[^{}]+(?=\{)', css))
    selectors = re.sub(QUOTED, '', selectors)
    classes = set(re.findall(r'\.('+SAFE_NAME+r')', selectors))
    for value in re.findall(r"\.className='([^']+)'", source):
        classes.update(value.split())
    head = source.split('R"HEAD(', 1)[1].split(')HEAD"', 1)[0]
    markup = head + body[:re.search(r'<script(?:\s[^>]*)?>', body, re.I).start()]
    ids = set()
    class Catalog(HTMLParser):
        def handle_starttag(self, tag, attrs):
            for name, value in attrs:
                if name == 'id' and value: ids.add(value)
                if name == 'class' and value: classes.update(value.split())
    catalog = Catalog()
    catalog.feed(markup)
    catalog.close()
    def allocate(names):
        frequency = Counter({name: len(re.findall(r'(?<![-\w])'+re.escape(name)+r'(?![-\w])', source)) for name in names})
        return {name: short_name(i) for i, name in enumerate(sorted(names, key=lambda n: (-frequency[n], n)))}
    return allocate(classes), allocate(ids)


def selector_names(selector, classes, ids):
    pattern = re.compile(QUOTED + r'|([.#])('+SAFE_NAME+r')')
    return pattern.sub(lambda m: m[1]+({'#': ids, '.': classes}[m[1]].get(m[2], m[2])) if m[1] else m[0], selector)


def css_names(css, classes, ids):
    # Declarations (including colors, strings and URLs) stay byte-for-byte intact.
    return re.sub(r'[^{}]+(?=\{)', lambda m: selector_names(m[0], classes, ids), css)


def tag_attributes(raw):
    """Yield attribute value spans, preserving quotes, inline code and entities."""
    cursor = re.match(r'<[^\s/>]+', raw).end()
    while cursor < len(raw):
        while cursor < len(raw) and raw[cursor].isspace(): cursor += 1
        if cursor == len(raw) or raw[cursor] in '/>': return
        start = cursor
        while cursor < len(raw) and not raw[cursor].isspace() and raw[cursor] not in '=/>': cursor += 1
        name = raw[start:cursor].lower()
        while cursor < len(raw) and raw[cursor].isspace(): cursor += 1
        if cursor == len(raw) or raw[cursor] != '=': continue
        cursor += 1
        while cursor < len(raw) and raw[cursor].isspace(): cursor += 1
        if cursor == len(raw): return
        quote = raw[cursor] if raw[cursor] in ('"', "'") else ''
        start = cursor + bool(quote)
        if quote:
            end = raw.find(quote, start)
            if end < 0: return
        else:
            end = start
            while end < len(raw) and not raw[end].isspace() and raw[end] != '>': end += 1
        yield name, start, end, quote
        cursor = end + bool(quote)


class HtmlNames(HTMLParser):
    def __init__(self, source, classes, ids):
        super().__init__(convert_charrefs=False)
        self.source, self.classes, self.ids = source, classes, ids
        self.offsets = [0] + [m.end() for m in re.finditer('\n', source)]
        self.edits = []
        self.feed(source)
        self.close()

    def handle_starttag(self, tag, attrs):
        raw = self.get_starttag_text()
        line, column = self.getpos()
        base = self.offsets[line - 1] + column
        for name, start, end, _ in tag_attributes(raw):
            value = raw[start:end]
            if name == 'class':
                replacement = ' '.join(self.classes.get(v, v) for v in value.split())
            elif name in ('id', 'for', 'aria-controls', 'aria-labelledby', 'aria-describedby', 'aria-owns', 'aria-activedescendant'):
                replacement = ' '.join(self.ids.get(v, v) for v in value.split())
            elif name == 'href' and value.startswith('#'):
                replacement = '#'+self.ids.get(value[1:], value[1:])
            else:
                continue
            if replacement != value:
                self.edits.append((base+start, base+end, replacement))

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)

    def result(self):
        result = self.source
        for start, end, replacement in reversed(self.edits):
            result = result[:start]+replacement+result[end:]
        return result


def html_names(html, classes, ids):
    return HtmlNames(html, classes, ids).result()


def script_names(script, classes, ids):
    # These are DOM API operands in the maintained portal source, not a general
    # JavaScript minifier. Protocol paths, page IDs and command values stay intact.
    def selector(m):
        literal = m[2]
        if '\\' in literal:
            raise ValueError('Escaped DOM selector needs explicit handling: '+literal)
        return m[1]+literal[0]+selector_names(literal[1:-1], classes, ids)+literal[-1]
    script = re.sub(r'(\.(?:querySelector|querySelectorAll|closest|matches)\()('+QUOTED+r')', selector, script)

    def class_value(literal):
        quote, value = literal[0], literal[1:-1]
        names = value.split()
        if any(name not in classes for name in names):
            raise ValueError('Unknown portal CSS class: '+value)
        return quote+' '.join(classes[name] for name in names)+quote
    # Class assignments are either a literal or the message-state ternary.
    assignments = len(re.findall(r'\.className\s*=(?!=)', script))
    script, rewritten = re.subn(r'(\.className\s*=\s*)(?:('+QUOTED+r')|([A-Za-z_][\w.]*\s*\?\s*)('+QUOTED+r')(\s*:\s*)('+QUOTED+r'))',
                    lambda m: m[1]+(class_value(m[2]) if m[2] else m[3]+class_value(m[4])+m[5]+class_value(m[6])), script)
    if rewritten != assignments:
        raise ValueError('Unsupported dynamic className assignment in portal source')
    script = re.sub(r'(\.classList\.(?:add|remove|toggle|contains)\()('+QUOTED+r')', lambda m: m[1]+class_value(m[2]), script)

    script = re.sub(r'(\.getElementById\()('+QUOTED+r')\)', lambda m: m[1]+json.dumps(ids.get(m[2][1:-1], m[2][1:-1]))+')', script)
    # e.target.id is a physical DOM ID, unlike currentPage.id and Zigbee actions.
    script = script.replace("e.target.id==='upgradeUrlForm'", 'e.target.id==='+json.dumps(ids['upgradeUrlForm']))

    dynamic = set()
    if '.getElementById(currentPage.id)' in script:
        dynamic.update(re.findall(r"\{id:'([^']+)',path:", script))
    if '.getElementById(id)' in script:
        dynamic.update(('rejoin', 'reset'))
    if re.search(r'\.querySelector\(`#\$\{[^}]+\}(?:Log|Tab|Panel)`\)', script):
        dynamic.update(name+suffix for name in ('mqtt', 'messages', 'zigbee', 'matter') for suffix in ('Log', 'Tab', 'Panel'))
    if dynamic:
        # Keep only semantic names used by indirect DOM lookups. Static selectors
        # and literal IDs need no runtime map. No map is stored in MCU RAM.
        entries = []
        for name in sorted(dynamic):
            if name not in ids: continue
            entry = json.dumps(name)+':'+json.dumps(ids[name])+','
            if name in ('rejoin', 'reset'):
                entry = '/*@if ARDPORTAL_ENABLE_ZIGBEE || ARDPORTAL_ENABLE_MATTER*/'+entry+'/*@endif*/'
            elif name == 'matter':
                entry = '/*@if ARDPORTAL_ENABLE_MATTER*/'+entry+'/*@endif*/'
            elif name.startswith('matter'):
                entry = '/*@if ARDPORTAL_ENABLE_MATTER_CONSOLE*/'+entry+'/*@endif*/'
            elif name.startswith('zigbee'):
                entry = '/*@if ARDPORTAL_ENABLE_ZIGBEE*/'+entry+'/*@endif*/'
            if name.startswith('messages'):
                # Console Messages has already been selected for this variant.
                if "'messages'" not in script: continue
            entries.append(entry)
        script = 'const _domIds={__proto__:null,'+''.join(entries)+'},_domId=n=>_domIds[n]??n;\n'+script
        script = script.replace('.getElementById(currentPage.id)', '.getElementById(_domId(currentPage.id))').replace('.getElementById(id)', '.getElementById(_domId(id))')
        script = re.sub(r'\.querySelector\(`#\$\{([^}]+)\}(Log|Tab|Panel)`\)', lambda m: ".querySelector('#'+_domId(("+m[1]+")+'"+m[2]+"'))", script)
    if "'dynamic-'+page.id" in script:
        script = script.replace(".getElementById('dynamic-'+page.id)", ".getElementById(_pageId(page))")
        script = script.replace("section.id='dynamic-'+page.id", "section.id=_pageId(page)")
        # Reserve underscore-prefixed two-character aliases for the 16 pages.
        # Static aliases always start with an ASCII letter.
        script = "/*@if ARDPORTAL_ENABLE_DYNAMIC_PAGES*/const _pageId=p=>'_'+'abcdefghijklmnop'[dynamicDefinitions.indexOf(p)];/*@endif*/\n"+script
    return script
