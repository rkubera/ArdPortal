#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Check packed message pools, feature removal and multiple language offsets."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'src/tools'))
from generate_languages import generate


def run(work, name, flags, body):
    source = work / (name + '.cpp')
    executable = work / name
    source.write_text('#define ARDUI_DEFINE_STORAGE_MESSAGES\n#include "Language.h"\n#include <cassert>\nuint32_t fakeMillis=0;\nint main(){\n' + body + '\n}\n')
    subprocess.run(['c++', '-std=c++17', '-Os', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I'+str(work),
                    '-I'+str(ROOT/'tests/host'), '-I'+str(ROOT/'src'),
                    *flags, str(source), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    return executable.read_bytes()


with tempfile.TemporaryDirectory(prefix='ardportal-language-') as directory:
    work = Path(directory)
    (work / 'Language.h').write_text((ROOT / 'src/Language.h').read_text())
    (work / 'LanguageData.h').write_text((ROOT / 'src/LanguageData.h').read_text())
    image = run(work, 'disabled', ['-DARDPORTAL_ENABLE_CONSOLE=0', '-DARDPORTAL_ENABLE_MQTT=0',
                                 '-DARDPORTAL_ENABLE_OTA=0', '-DARDPORTAL_ENABLE_WIFI=0'], r'''
    static_assert(sizeof(ArdUILanguageData::Message)==4,"single-language records must use four bytes");
    using K=ArdUILanguage::Key;
    assert(!ArdUILanguage::textFlash(K::s_101));
    assert(!ArdUILanguage::textFlash(K::s_148));
    assert(!ArdUILanguage::textFlash(K::s_117));
    assert(!ArdUILanguage::textFlash(K::s_178));
    assert(!ArdUILanguage::textFlash(K::s_157));
    assert(ArdUILanguage::text(K::s_120)=="Invalid request.");
    assert(!ArdUILanguage::textFlash(static_cast<K>(0)));
    ''')
    for text in ('Wi-Fi IP: ', 'MQTT connected.', 'OTA started.', 'Binary bytes: ', 'MQTT RX: '):
        assert text.encode()+b'\0' not in image, 'Disabled message retained: '+text
    run(work, 'serial', ['-DARDPORTAL_ENABLE_CONSOLE=0', '-DARDPORTAL_MQTT_DEBUG_SERIAL=1'], r'''
    using K=ArdUILanguage::Key;
    assert(ArdUILanguage::text(K::s_146)=="TX queued");
    assert(ArdUILanguage::text(K::s_178)=="Binary bytes: ");
    ''')
    catalogs = work / 'languages'
    catalogs.mkdir()
    for code, strings in [('en', {'ui_001':'Test','s_120':'Shared','s_132':'Shared','s_194':'Storage'}),
                          ('pl', {'ui_001':'Test','s_120':'Żółć','s_132':'Żółć','s_194':'Dane'})]:
        # Include the complete storage ID set expected by the standalone ArdFS ABI.
        for n in (195,196,198,199,200,204): strings['s_'+str(n)] = 'Storage'
        (catalogs/(code+'.json')).write_text(json.dumps({'code':code,'name':code,'locale':code,'strings':strings}))
    (work/'LanguageData.h').write_text(generate(catalogs))
    run(work, 'multilingual', [], r'''
    static_assert(sizeof(ArdUILanguageData::Message)==6,"two-language records must use six bytes");
    using K=ArdUILanguage::Key;
    assert(ArdUILanguage::text(K::s_120)=="Shared");
    assert(ArdUILanguage::textFlash(K::s_120)==ArdUILanguage::textFlash(K::s_132));
    assert(ArdUILanguage::select("pl"));
    assert(ArdUILanguage::text(K::s_120)=="Żółć");
    assert(ArdUILanguage::text(K::s_194)=="Dane");
    assert(ArdUILanguage::textFlash(K::s_120)==ArdUILanguage::textFlash(K::s_132));
    ArdUILanguage::active()=999;
    assert(ArdUILanguage::text(K::s_120)=="Shared");
    assert(ArdUILanguage::storageText(K::s_194)=="Storage");
    assert(ArdUILanguage::text(static_cast<K>(65535))=="s_65535");
    ''')
print('PASS: packed language records, shared translations, UTF-8 offsets, storage pools, fallback and disabled/serial profiles')
