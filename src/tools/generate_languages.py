#!/usr/bin/env python3
# Author: Radoslaw Kubera (rkubera on GitHub).
# SPDX-License-Identifier: MIT
"""Compile languages/*.json into flash-resident C++ resources (no device FS upload)."""
import argparse
import gzip
import json
import re
from pathlib import Path
from cpp_format import format_cpp
from control_assets import emit_chunks, condition_enabled
from generate_portal_assets import gzip_array


# @brief Build a JSON object while rejecting duplicate keys.
# @param pairs Ordered JSON key/value pairs; duplicate keys are rejected.
# @return A dictionary of the pairs; raises on duplicate keys.
def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


FEATURE_KEYS = json.loads(Path(__file__).with_name('language_features.json').read_text(encoding='utf-8'))
CONTROL_KEYS = json.loads(Path(__file__).with_name('control_language_features.json').read_text())
UI_VARIANTS = [(1, 1, 1), (1, 1, 0), (1, 0, 1), (1, 0, 0), (0, 1, 1), (0, 1, 0), (0, 0, 1), (0, 0, 0)]
STORAGE_KEYS = {f's_{number}' for number in (194,195,196,198,199,200,204)}


# @brief Collect the localized UI strings used by the portal source.
# @param strings Localized UI string collection.
# @param ota Whether OTA UI is included.
# @param mqtt Whether MQTT UI is included.
# @param console Whether Console UI is included.
# @param controls Optional set of controls included in the asset.
# @param dynamic Whether dynamic-page UI is included.
# @param tls Whether MQTT TLS UI is included.
# @return The collected UI string definitions.
def ui_strings(strings, ota=1, mqtt=1, console=1, controls=None, dynamic=True, tls=True, zigbee=True, matter=False):
    enabled = {'OTA': ota, 'MQTT': mqtt, 'CONSOLE': console, 'HA': True, 'DEPENDENCIES': dynamic, 'DYNAMIC_PAGES': dynamic, 'WEBSOCKET': dynamic or console, 'MQTT_TLS':tls and mqtt, 'ZIGBEE':zigbee, 'MATTER':matter}
    excluded = {key for feature, keys in FEATURE_KEYS.items() if not enabled[feature] for key in keys}
    return {key: value for key, value in strings.items() if key.startswith('ui_') and key not in excluded and condition_enabled(condition_for(key), controls, enabled)}


# @brief Build the language payload for the selected UI features.
# @param lines Generated source lines to wrap in a feature guard.
# @param name Fallback name or generated symbol name.
# @param strings Localized UI string collection.
# @param flags Enabled UI feature flags.
# @param controls Optional set of controls included in the asset.
# @param dynamic Whether dynamic-page UI is included.
# @return Serialized UI language data for the selected features.
def ui_payload(lines,name,strings,flags,controls=None,dynamic=True):
    lines.append('#if ARDPORTAL_ENABLE_MQTT_TLS')
    for tls in [True,False]:
        if not tls:lines.append('#else')
        for guard,zigbee,matter in [('#if ARDPORTAL_ENABLE_ZIGBEE',True,False),('#elif ARDPORTAL_ENABLE_MATTER',False,True),('#else',False,False)]:
            lines.append(guard)
            payload=json.dumps(ui_strings(strings,*flags,controls=controls,dynamic=dynamic,tls=tls,zigbee=zigbee,matter=matter),ensure_ascii=False,separators=(',',':'))
            lines.extend(emit_chunks(name,[('1',payload)],gzip_array))
        lines.append('#endif')
    lines.append('#endif')


# @brief Resolve the feature controlling a localized string.
# @param key Configuration key or JSON object member name.
# @return The feature condition associated with the string.
def feature_for(key):
    return next((feature for feature, keys in FEATURE_KEYS.items() if key in keys), None)


# @brief Build the compile-time condition selecting a language asset.
# @param key Configuration key or JSON object member name.
# @return The C++ feature expression selecting the asset variant.
def condition_for(key):
    # Native diagnostics follow their actual consumers, not the Console UI.
    # MQTT serial logging can be enabled independently of that UI.
    if key == 's_157': return '0'  # obsolete MQTT RX prefix, no caller
    if key in {'s_188','s_167','s_168','s_176'}:
        return 'ARDPORTAL_ENABLE_CONSOLE_MESSAGES'
    if key == 's_186': return 'defined(ESP32)'
    if key in {'s_101','s_102','s_103','s_104','s_105','s_106','s_209','s_210'}:
        return 'ARDPORTAL_ENABLE_WIFI && ARDPORTAL_ENABLE_CONSOLE_MESSAGES'
    if key in {'s_148','s_149','s_151'}:
        return 'ARDPORTAL_ENABLE_MQTT && ARDPORTAL_ENABLE_CONSOLE_MESSAGES'
    if key == 's_117': return 'ARDPORTAL_HTTP_OTA_SUPPORT && ARDPORTAL_ENABLE_CONSOLE_MESSAGES'
    if key in {'s_116','s_152','s_153','s_155'}: return 'ARDPORTAL_HTTP_OTA_SUPPORT'
    if key == 's_146': return 'ARDPORTAL_ENABLE_MQTT && (ARDPORTAL_ENABLE_CONSOLE || ARDPORTAL_MQTT_DEBUG_SERIAL || ARDPORTAL_ENABLE_LOG_CALLBACK)'
    if key == 's_178': return 'ARDPORTAL_ENABLE_CONSOLE || ARDPORTAL_MQTT_DEBUG_SERIAL || (ARDPORTAL_ENABLE_LOG_CALLBACK && ARDPORTAL_ENABLE_MQTT)'
    if key == 's_164': return 'ARDPORTAL_ENABLE_MQTT && ARDPORTAL_ENABLE_CONSOLE'
    if key in {'s_109','s_124','s_128','s_129','s_131','s_142','s_174','s_179','s_211'}:
        return 'ARDPORTAL_ENABLE_WWW'
    feature = feature_for(key)
    if feature == 'WEBSOCKET':
        return 'ARDPORTAL_SUPPORT_WEBSOCKET'
    if feature:
        return 'ARDPORTAL_ENABLE_' + feature
    if key in CONTROL_KEYS:
        condition = ' || '.join('ARDPORTAL_ENABLE_CONTROL_' + c.upper() for c in CONTROL_KEYS[key])
        if key == 'ui_050':
            condition += ' || ARDPORTAL_ENABLE_CONSOLE'
        return condition
    return '1'


# @brief Wrap generated lines in their feature condition.
# @param lines Generated source lines to wrap in a feature guard.
# @param key Configuration key or JSON object member name.
# @param body HTTP or MQTT message body.
# @return Generated lines wrapped in the requested feature condition.
def guarded(lines, key, body):
    condition = condition_for(key)
    if condition != '1':
        lines.append('#if ' + condition)
    lines.extend(body)
    if condition != '1':
        lines.append('#endif')


# @brief Build the generated header text from its source definitions.
# @param directory Directory containing language source files.
# @return Complete generated C++ header text.
def generate(directory):
    paths = sorted(directory.glob("*.json"), key=lambda path: (path.stem != "en", path.name))
    if not paths:
        raise ValueError("At least one language file is required")
    languages = []
    for path in paths:
        data = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique_object)
        if set(data) != {"code", "name", "locale", "strings"}:
            raise ValueError(f"{path.name}: expected code, name, locale and strings")
        if not isinstance(data["code"], str) or not re.fullmatch(r"[a-z]{2,3}(?:-[A-Za-z0-9]{2,8})?", data["code"]) or data["code"] != path.stem:
            raise ValueError(f"{path.name}: invalid code or filename")
        for field in ("name", "locale"):
            if not isinstance(data[field], str) or not data[field] or len(data[field]) > 64:
                raise ValueError(f"{path.name}: invalid {field}")
        if not isinstance(data["strings"], dict) or not data["strings"]:
            raise ValueError(f"{path.name}: strings must be a nonempty object")
        for key, value in data["strings"].items():
            if not re.fullmatch(r"[a-z][a-z0-9_]{0,63}", key) or not isinstance(value, str) or "\0" in value:
                raise ValueError(f"{path.name}: invalid string {key}")
        languages.append(data)
    base = languages[0]["strings"]
    placeholders = lambda value: set(re.findall(r"\{([a-zA-Z][a-zA-Z0-9_]*)\}", value))
    for language in languages:
        extra = set(language["strings"]) - set(base)
        if extra:
            raise ValueError(f"{language['code']}: unknown keys {sorted(extra)}")
        for key, value in language["strings"].items():
            if placeholders(value) != placeholders(base[key]):
                raise ValueError(f"{language['code']}: placeholders differ for {key}")
        language["strings"] = {**base, **language["strings"]}
    quote = lambda value: json.dumps(value, ensure_ascii=False)
    lines = ['// Author: Radoslaw Kubera (rkubera on GitHub).', '// SPDX-License-Identifier: MIT',
             "// Generated by tools/generate_languages.py. Edit languages/*.json instead.",
             "#pragma once", "#include <Arduino.h>", "#include <stddef.h>", '#include "ArdPortalFeatures.h"', '#include "ArdAsset.h"', "namespace ArdUILanguageData {",
             f"static constexpr size_t Count = {len(languages)};"]
    manifest = {"default": languages[0]["code"], "languages": [
        {key: language[key] for key in ("code", "name", "locale")} for language in languages]}
    lines.append('static const char Manifest[] PROGMEM = ' + quote(json.dumps(manifest, ensure_ascii=False, separators=(",", ":"))) + ';')
    # Optional caption groups form one gzip stream. The browser sees one JSON
    # object; selected DEFLATE blocks stream directly from flash.
    for i, language in enumerate(languages):
        groups = {}
        for key, value in language['strings'].items():
            if key.startswith('ui_'):
                groups.setdefault(condition_for(key), {})[key] = value
        core = groups.pop('1')
        parts = [('1', json.dumps(core, ensure_ascii=False, separators=(',', ':'))[:-1])]
        for condition, values in groups.items():
            parts.append((condition, ',' + json.dumps(values, ensure_ascii=False, separators=(',', ':'))[1:-1]))
        parts.append(('1', '}'))
        lines.append('#if !ARDPORTAL_ENABLE_DYNAMIC_PAGES')
        for variant, flags in enumerate(UI_VARIANTS):
            condition = ' && '.join(('' if value else '!') + 'ARDPORTAL_ENABLE_' + feature for feature,value in zip(('OTA','MQTT','CONSOLE'),flags))
            lines.append(('#if ' if variant==0 else '#elif ')+condition)
            ui_payload(lines,'Json'+str(i),language['strings'],flags,[],False)
        lines += ['#endif','#elif ARDPORTAL_CONTROL_PROFILE_FULL || ARDPORTAL_CONTROL_PROFILE_BASIC']
        for variant, flags in enumerate(UI_VARIANTS):
            condition = ' && '.join(('' if value else '!') + 'ARDPORTAL_ENABLE_' + feature
                                    for feature, value in zip(('OTA','MQTT','CONSOLE'),flags))
            lines.append(('#if ' if variant == 0 else '#elif ') + condition)
            lines.append('#if ARDPORTAL_CONTROL_PROFILE_FULL')
            for controls in (None,['slider','text','select','edit']):
                if controls is not None: lines.append('#else')
                ui_payload(lines,'Json'+str(i),language['strings'],flags,controls)
            lines.append('#endif')
        lines += ['#endif','#else']
        lines.extend(emit_chunks('Json'+str(i), parts, gzip_array))
        lines.append('#endif')
    for i, language in enumerate(languages):
        for field in ('code', 'name', 'locale'):
            lines.append(f'static const char {field}{i}[] PROGMEM = {quote(language[field])};')
    lines.append('struct Language { const char *code, *name, *locale; const ArdAssetChunk* json; size_t count; };')
    lines.append('static const Language Languages[] PROGMEM = {')
    for i in range(len(languages)):
        lines.append(f'  {{code{i}, name{i}, locale{i}, Json{i}, sizeof(Json{i}) / sizeof(Json{i}[0])}},')
    lines.append('};')
    native = [key for key in base if key.startswith('s_')]
    for key in native:
        if not re.fullmatch(r's_[1-9][0-9]*',key) or int(key[2:])>65535:
            raise ValueError('Native message IDs must be canonical s_1..s_65535: '+key)
    lines.append('enum class Key : uint16_t { '+', '.join(key+' = '+key[2:] for key in native)+' };')
    # Byte offsets avoid pointer-sized entries and let equal translations share
    # one flash string. Each table owns its pool; standalone ArdFS retains only
    # storage text, independently of the sketch's feature selection.
    lines.append('struct Message { uint16_t id; uint16_t offsets[Count]; };')
    for name, storage in [('Messages', False), ('StorageMessages', True)]:
        keys = [key for key in native if (key in STORAGE_KEYS) == storage]
        values = {}
        for key in keys:
            for j, language in enumerate(languages):
                text = language['strings'][key]
                values.setdefault(text, []).append((key, j))
        members = {}
        lines.append('struct ' + name + 'Text {')
        for i, (text, uses) in enumerate(values.items()):
            conditions = list(dict.fromkeys(condition_for(key) for key, _ in uses))
            condition = '1' if '1' in conditions else ' || '.join('('+c+')' for c in conditions)
            for use in uses:
                members[use] = i
            if condition != '1': lines.append('#if ' + condition)
            lines.append(f'char v{i}[{len(text.encode("utf-8"))+1}];')
            if condition != '1': lines.append('#endif')
        lines += ['char end;', '};', 'static_assert(sizeof('+name+'Text) <= 65536, "Message offsets must fit uint16_t");']
        if storage:
            lines += [f'extern const Message {name}[{len(keys)}] PROGMEM;', 'extern const '+name+'Text '+name+'Pool PROGMEM;', '#ifdef ARDUI_DEFINE_STORAGE_MESSAGES']
        lines.append(('const' if storage else 'static const') + ' ' + name + 'Text ' + name + 'Pool PROGMEM = {')
        for text, uses in values.items():
            conditions = list(dict.fromkeys(condition_for(key) for key, _ in uses))
            condition = '1' if '1' in conditions else ' || '.join('('+c+')' for c in conditions)
            if condition != '1': lines.append('#if ' + condition)
            lines.append(quote(text)+',')
            if condition != '1': lines.append('#endif')
        lines.append('0};')
        lines.append(('const' if storage else 'static const') + ' Message ' + name + '[] PROGMEM = {')
        for key in keys:
            offsets = ', '.join(f'uint16_t(offsetof({name}Text, v{members[key,j]}))' for j in range(len(languages)))
            guarded(lines, key, ['  {' + key[2:] + ', {' + offsets + '}},'])
        # Keep zero-feature builds standard C++; ID zero is never a valid key.
        if not storage: lines.append('{0, {0}},')
        lines.append('};')
        if storage: lines.append('#endif')
    lines += ['} // namespace ArdUILanguageData', '']
    return '\n'.join(lines)


# @brief Parse command-line options and generate or verify the output file.
# @return No value; exits with an error when generation or verification fails.
def native_literals(directory):
    """Keep low-level const-char, F(), printf and static_assert messages as literals.

    No pointer table, String allocation or runtime language state is introduced.
    The selected native language is fixed at compilation; UI messages remain dynamic.
    """
    paths = sorted(directory.glob('*.json'), key=lambda path: (path.stem != 'en', path.name))
    catalogs = [json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique_object) for path in paths]
    base = catalogs[0]['strings']
    keys = [key for key in base if key.startswith('native_')]
    lines = ['// Generated by tools/generate_languages.py. Edit languages/*.json instead.',
             '// Author: Radoslaw Kubera (rkubera on GitHub).', '// SPDX-License-Identifier: MIT',
             '#pragma once', '#define ARD_LANG_PRAGMA_IMPL(value) _Pragma(#value)',
             '#define ARD_LANG_COMPILE_ERROR(value) ARD_LANG_PRAGMA_IMPL(GCC error value)', '#ifndef ARDPORTAL_NATIVE_LANGUAGE', '#define ARDPORTAL_NATIVE_LANGUAGE 0', '#endif']
    for index, catalog in enumerate(catalogs):
        lines.append(('#if' if index == 0 else '#elif') + f' ARDPORTAL_NATIVE_LANGUAGE == {index} // {catalog["code"]}')
        for key in keys:
            value = catalog['strings'].get(key, base[key])
            formats = lambda text: re.findall(r'%(?:[-+ #0]*\d*(?:\.\d+)?(?:hh|ll|[hljztL])?[diuoxXfFeEgGaAcsp]|%)', text)
            if formats(value) != formats(base[key]):
                raise ValueError(f'{catalog["code"]}: printf formats differ for {key}')
            lines.append('#define ARD_LANG_' + key.upper() + ' ' + json.dumps(value, ensure_ascii=False))
    lines += ['#else', '#error ' + json.dumps(base['native_unsupported_native_language_index']), '#endif', '']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument('--directory', type=Path, default=root / 'languages')
    parser.add_argument('--output', type=Path, default=root / 'LanguageData.h')
    parser.add_argument('--literals-output', type=Path, default=root / 'LanguageLiterals.h')
    parser.add_argument('--check', action='store_true', help='Fail if generated resources are stale')
    args = parser.parse_args()
    output = format_cpp(generate(args.directory), "LanguageData.h")
    literals = format_cpp(native_literals(args.directory), "LanguageLiterals.h")
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding='utf-8') != output:
            raise SystemExit('LanguageData.h is stale; run tools/generate_languages.py')
        if not args.literals_output.exists() or args.literals_output.read_text(encoding='utf-8') != literals:
            raise SystemExit('LanguageLiterals.h is stale; run tools/generate_languages.py')
    else:
        args.output.write_text(output, encoding='utf-8')
        args.literals_output.write_text(literals, encoding='utf-8')
        print(f'Generated {args.output}')


if __name__ == '__main__':
    main()
