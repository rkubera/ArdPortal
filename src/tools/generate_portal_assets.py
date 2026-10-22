#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Compress portal CSS/JavaScript deterministically using Python's standard library."""
import argparse
import gzip
import re
from html.parser import HTMLParser
from control_assets import segments, selected, emit_profile_chunks, emit_chunks, segments
from pathlib import Path


class PortalSections(HTMLParser):
    """Locate inline asset spans while preserving the original source bytes.

    This extracts trusted build input; it is not an HTML sanitizer.
    """

    def __init__(self, source):
        super().__init__(convert_charrefs=False)
        self.source = source
        self.line_offsets = [0]
        for match in re.finditer("\n", source):
            self.line_offsets.append(match.end())
        self.sections = {}
        self.active = None
        self.feed(source)
        self.close()
        if self.active or set(self.sections) != {"style", "script"}:
            raise ValueError("Portal HTML must contain one complete style and script element")

    def source_offset(self):
        line, column = self.getpos()
        return self.line_offsets[line - 1] + column

    def handle_starttag(self, tag, attrs):
        if tag not in {"style", "script"} or self.active:
            return
        if tag in self.sections or any(name == "src" for name, _ in attrs):
            raise ValueError("Portal assets must be unique inline elements")
        start = self.source_offset()
        self.active = (tag, start, start + len(self.get_starttag_text()))

    def handle_endtag(self, tag):
        if not self.active or self.active[0] != tag:
            return
        _, start, content_start = self.active
        content_end = self.source_offset()
        end = self.source.index(">", content_end) + 1
        self.sections[tag] = (start, content_start, content_end, end)
        self.active = None

    def content(self, tag):
        _, start, end, _ = self.sections[tag]
        return self.source[start:end]


# @brief Select portal HTML, CSS and JavaScript for the requested feature combination.
# @param root Directory containing the asset source files.
# @param ota Whether OTA UI is included.
# @param mqtt Whether MQTT UI is included.
# @param console Whether Console UI is included.
# @param controls Optional set of controls included in the asset.
# @param guarded Whether to retain feature guards for generation.
# @param tls Whether MQTT TLS UI is included.
# @param messages Whether diagnostic Messages UI is included.
# @return A tuple of CSS text, JavaScript text and the HTML shell for the selected features.
def portal_parts(root, ota=True, mqtt=True, console=True, controls=None, guarded=False, tls=True, messages=True):
    """Return the selected UI; excluded HTML and JS are absent from its asset."""
    source = (root / 'PortalPage.h').read_text(encoding='utf-8')
    body = source.split('R"HTML(', 1)[1].split(')HTML"', 1)[0]
    for enabled, ids in [(ota, ['upgrade']), (mqtt, ['mqtt']), (console, ['console'])]:
        if not enabled:
            for section in ids:
                body = re.sub(r'<section id="' + section + r'"[\s\S]*?</section>', '', body)
    body=re.sub(r'/\*@if ARDPORTAL_ENABLE_MQTT_TLS\*/(.*?)/\*@endif\*/',lambda m:m[1] if tls and mqtt else '',body,flags=re.S)
    if not messages:
        body = re.sub(r'<button[^>]*id="messagesTab".*?</button>', '', body, flags=re.S)
        body = re.sub(r'<div id="messagesPanel".*?</div>', '', body, flags=re.S)
    sections = PortalSections(body)
    css = sections.content('style')
    script = sections.content('script')
    # Keep source documentation out of firmware while retaining feature guards.
    script = re.sub(r'(?m)^[ \t]*/\*\*(?:(?!\*/).)*\*/\n?', '', script, flags=re.S)
    for enabled, entry in [(ota, 'upgrade'), (mqtt, 'mqtt'), (console, 'console')]:
        if not enabled:
            script = re.sub(r",?\{id:'" + entry + r"',path:'[^']+',label:'[^']+'\}", '', script)
    if not ota:
        script = '\n'.join(line for line in script.split('\n')
                           if not line.startswith("document.querySelector('#upgradeForm').onsubmit")
                           and not line.startswith("document.querySelector('#upgradeUrlForm').onsubmit"))
    if not messages:
        pass
    if not console:
        script = script.replace("else document.querySelector('#publishForm').elements.topic.placeholder=t('ui_097',{device:document.querySelector('#publishForm').dataset.device})", "")
        script = script.replace("document.querySelector('#publishForm').dataset.device=c.mqttName;", "")
        script = script.replace("document.querySelector('#publishForm').elements.topic.placeholder=t('ui_097',{device:c.mqttName});", "")
        script = script.replace("if(currentPage?.id==='console')setMessage(document.querySelector('#consoleState'),consoleErrorKey?t(consoleErrorKey):'',!!consoleErrorKey);", "")
        script = re.sub(r"const consoleRows=\{[^;]+;", "", script)
        script = re.sub(r"^function consoleAvailability\(\).*?$", 'function consoleAvailability(){}', script, flags=re.M)
        script = '\n'.join(line for line in script.split('\n')
                           if not line.startswith("document.querySelector('#publishForm').onsubmit")
                           and not line.startswith("if(currentPage?.id==='console'){consoleAvailability();")
                           and not line.startswith("function selectConsoleTab("))
        script = script.replace("setMessage(document.querySelector('#consoleState'),", "setConsoleMessage(")
        script = script.replace("function openConsole()", "function setConsoleMessage(){}\nfunction openConsole()")
        script = script.replace("currentPage?.id==='console'?'/api/console':'/api/events'", "'/api/events'")
        # Only application value/status notifications remain in the WebSocket receiver.
        start = script.index("else if(message.type==='log'")
        end = script.index("}catch(error)", start)
        script = script[:start] + script[end:]
    if not mqtt:
        script = script.replace("status.textContent=t('ui_098'", "status.textContent=t('ui_198'")
        script = script.replace("[t('ui_181'),i.mqtt?t('ui_078'):t('ui_079')],", "")
        body = re.sub(r'<button[^>]*id="mqttTab".*?</button>', '', body, flags=re.S)
        body = re.sub(r'<div id="mqttPanel".*?</div>', '', body, flags=re.S)
        script = script.replace("document.querySelector('#publishForm').dataset.device=c.mqttName;", "")
        script = script.replace("document.querySelector('#publishForm').elements.topic.placeholder=t('ui_097',{device:c.mqttName});", "")
        script = script.replace("else document.querySelector('#publishForm').elements.topic.placeholder=t('ui_097',{device:document.querySelector('#publishForm').dataset.device})", "")
        script = '\n'.join(line for line in script.split('\n') if not line.startswith("document.querySelector('#publishForm').onsubmit"))
        script = script.replace("mqttForm=document.querySelector('#mqttForm')", "mqttForm=null")
        script = '\n'.join(line for line in script.split('\n')
                           if not line.startswith('function setCaVisibility')
                           and not line.startswith('mqttForm.elements.mqttTls.onchange'))
        script = script.replace('setCaVisibility()', '')
    script = re.sub(r'/\*@if ARDPORTAL_ENABLE_CONSOLE_MESSAGES\*/(.*?)/\*@endif\*/', lambda m: m[1] if messages and console else '', script, flags=re.S)
    sections = PortalSections(body)
    script_start, _, _, script_end = sections.sections['script']
    selected_body = body[:script_start] + '<script>' + script + '</script>' + body[script_end:]
    style_start, _, _, style_end = sections.sections['style']
    shell = selected_body.replace(body[style_start:style_end], '<link rel="stylesheet" href="/portal.css">', 1)
    shell = shell.replace('<script>' + script + '</script>', '<script src="/portal.js"></script>')
    return (css, script, shell) if guarded else (selected(css, controls), selected(script, controls), shell)


# @brief Encode deterministic gzip data as a C++ PROGMEM string array.
# @param name Fallback name or generated symbol name.
# @param text Text to read, encode or display.
# @return C++ source lines containing the deterministic gzip byte array.
def gzip_array(name, text):
    data = bytearray(gzip.compress(text.encode('utf-8'), compresslevel=9, mtime=0))
    data[9] = 255
    lines = ['static const char ' + name + '[] PROGMEM =']
    for offset in range(0, len(data), 32):
        lines.append('"' + ''.join('\\x%02x' % byte for byte in data[offset:offset + 32]) + '"')
    lines[-1] += ';'
    return lines


# @brief Build the generated header text from its source definitions.
# @param root Directory containing the asset source files.
# @return Complete generated C++ header text.
def generate(root):
    lines = ['// Author: Radoslaw Kubera (rkubera on GitHub).', '// SPDX-License-Identifier: MIT',
             '// Generated by tools/generate_portal_assets.py. Edit PortalPage.h instead.',
             '#pragma once', '#include <Arduino.h>', '#include "ArdPortalFeatures.h"', '#include "ArdAsset.h"']
    css, _, _ = portal_parts(root, guarded=True)
    lines += emit_profile_chunks('PORTAL_CSS_GZIP', css, gzip_array)
    variants = [(1, 1, 1), (1, 1, 0), (1, 0, 1), (1, 0, 0), (0, 1, 1), (0, 1, 0), (0, 0, 1), (0, 0, 0)]
    variants=[(*flags,tls) for flags in variants for tls in ([1,0] if flags[1] else [0])]
    variants=[(*flags,messages) for flags in variants for messages in ([1,0] if flags[2] else [0])]
    for index, (ota, mqtt, console, tls, messages) in enumerate(variants):
        flags = [('OTA', ota), ('MQTT', mqtt), ('CONSOLE', console), ('MQTT_TLS',tls), ('CONSOLE_MESSAGES',messages)]
        condition = ' && '.join(('' if value else '!') + 'ARDPORTAL_ENABLE_' + name for name, value in flags)
        lines.append(('#if ' if not index else '#elif ') + condition)
        _, script, shell = portal_parts(root, ota, mqtt, console, guarded=True,tls=tls,messages=messages)
        head=(root/'PortalPage.h').read_text().split('R"HEAD(',1)[1].split(')HEAD"',1)[0].split('<script id="pageCatalog"',1)[0]
        minimal=head+shell.removeprefix('</script>')
        minimal=re.sub(r'<section id="dynamicLoading".*?</section>','',minimal,flags=re.S)
        minimal=re.sub(r'<nav id="dynamicNavigation".*?</nav>','',minimal,flags=re.S)
        lines += ['#if ARDPORTAL_ENABLE_DYNAMIC_PAGES','static const char PORTAL_HTML[] PROGMEM =']
        for guard,fragment in segments(shell):
            if guard!='1':lines.append('#if '+guard)
            lines.append('R"ASSET('+fragment+')ASSET"')
            if guard!='1':lines.append('#endif')
        lines+=[';','#else']
        lines += emit_chunks('PORTAL_HTML_GZIP',segments(minimal),gzip_array)
        lines += ['#endif']
        lines += emit_profile_chunks('PORTAL_JS_GZIP', script, gzip_array)
    lines += ['#else', 'ARD_LANG_COMPILE_ERROR(ARD_LANG_NATIVE_INVALID_UI_FEATURE_COMBINATION)', '#endif']
    return '\n'.join(lines) + '\n'


# @brief Parse command-line options and generate or verify the output file.
# @return No value; exits with an error when generation or verification fails.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    target = root / 'PortalAssets.h'
    output = generate(root)
    if args.check:
        if not target.exists() or target.read_text(encoding='utf-8') != output:
            raise SystemExit('PortalAssets.h is stale; run tools/generate_portal_assets.py')
    else:
        target.write_text(output, encoding='utf-8')


if __name__ == '__main__':
    main()
