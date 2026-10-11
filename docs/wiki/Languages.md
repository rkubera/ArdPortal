# Languages and translations

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## Multilingual names in page JSON

Use `name` for the default label and `names` for language-specific labels. Keys such as `en`, `pl` and `de` match the compiled language catalog codes. Page names, field names and dropdown option names can all have separate translations:

```json
{
  "id":"settings",
  "name":"Settings",
  "names":{"en":"Settings","pl":"Ustawienia","de":"Einstellungen"},
  "fields":[
    {
      "id":"enabled","type":"switch","name":"Enabled",
      "names":{"en":"Enabled","pl":"Włączone","de":"Aktiviert"},
      "default":true
    },
    {
      "id":"profile","type":"select","name":"Profile",
      "names":{"en":"Profile","pl":"Profil","de":"Profil"},
      "options":[
        {"value":"eco","name":"Economy","names":{"en":"Economy","pl":"Oszczędny","de":"Sparmodus"}},
        {"value":"comfort","name":"Comfort","names":{"en":"Comfort","pl":"Komfort","de":"Komfort"}}
      ],
      "default":"eco"
    }
  ]
}
```

With English selected, this page displays **Settings / Enabled / Profile / Economy**. With Polish selected it displays **Ustawienia / Włączone / Profil / Oszczędny**. With German selected it displays **Einstellungen / Aktiviert / Profil / Sparmodus**. Compile the corresponding language catalogs as described below; adding `pl` or `de` to a page definition alone does not add a language to the selector. A missing localized label falls back to `name`.

Keep `id`, option `value`, MQTT topics and JSON property keys unchanged between languages. They are application/protocol identifiers. Only the displayed text changes. For example, selecting **Oszczędny** still produces the value `eco`.

Individual controls within a composite field can also use translated names:

```json
{
  "id":"fan","type":"fan","name":"Fan",
  "names":{"en":"Fan","pl":"Wentylator","de":"Ventilator"},
  "default":{"state":"OFF","percentage":0},
  "controls":[
    {"key":"state","type":"switch","command":"","on":"ON","off":"OFF","name":"Power","names":{"en":"Power","pl":"Zasilanie","de":"Ein/Aus"}},
    {"key":"percentage","type":"slider","command":"percentage","min":0,"max":100,"step":1,"name":"Speed","names":{"en":"Speed","pl":"Prędkość","de":"Geschwindigkeit"}}
  ]
}
```

For grouped scene buttons, use `group.names` to translate the shared group title and the field's `names` for each button:

```json
{
  "id":"morning","type":"scene","name":"Morning",
  "names":{"en":"Morning","pl":"Poranek","de":"Morgen"},
  "group":{"id":"scenes","name":"Scenes","names":{"en":"Scenes","pl":"Sceny","de":"Szenen"}}
}
```

## Languages and adding translations

Translations are compiled according to the control flags and `ARDPORTAL_ENABLE_OTA`,
`ARDPORTAL_ENABLE_MQTT` and `ARDPORTAL_ENABLE_CONSOLE`, using the same flags as
implementation and page assets. Disabled modules contribute neither their
browser dictionary entries nor their native message entries. MQTT automatically
disables Console and HA. Shared Info and dynamic-control translations remain
available when HA is off; they belong to the local portal, not discovery.

All translations stay in `languages/<code>.json`. The module-to-key assignment
is in `tools/language_features.json`; keys absent from this map are shared.
When adding a module-specific key, add it to the corresponding assignment and
regenerate `LanguageData.h`. Additional languages inherit the same assignments
and the English fallback. ArdFS uses a separate storage-message table so its
independent compilation cannot retain text from disabled portal modules.

Info labels, connection/status descriptions, units, network list captions and
portal error messages are sourced from the language catalog. Device names,
SSIDs, MQTT topics/payloads and identifiers such as ESP8266, ArdFS and ArdJSON
are data or product names and are displayed unchanged. Custom dynamic page
labels use their optional `names` translations and fall back to `name`.

All portal descriptions, labels, confirmations and native system messages are
configured in one JSON file per language:

- `src/languages/en.json` — English

The library includes only English, so the language selector is hidden by default.
Dynamic examples use English default names. Additional languages can still be added.

The top-right selector shows compact language-code buttons such as `EN` and `DE`,
with the active language highlighted. It appears only when more than one language
is compiled. A manual selection is remembered in browser localStorage. On the
first visit, the portal checks the browser’s ordered `navigator.languages` list
and `navigator.language`, matching locale/code first and then the base language.
An available saved preference takes priority; otherwise the default catalog is
the final fallback. Automatic detection does not overwrite a manual preference. Changing language preserves unsaved form values and does not write
flash. HTTP requests carry `X-ArdUI-Language`; future native messages use the
language of the most recent portal request. Application-defined text and generic
JSON parser error codes are outside this translation catalog.

To add another language:

1. Copy `en.json` to a file named after the language code, for example `de.json`.
2. Set `code` to `de`, `name` to `Deutsch`, and `locale` to `de-DE`.
3. Translate `strings`, retaining every key and `{placeholder}` name.
4. From the project root, run `python3 src/tools/generate_languages.py`.
5. Recompile/upload the firmware. No separate filesystem upload is needed.

From an installed library directory, run `python3 src/tools/generate_languages.py`.
The generator discovers all `languages/*.json`, validates filenames/metadata,
duplicate keys and placeholders, and creates flash-resident `LanguageData.h`.
Do not edit that generated file. Missing translations fall back to the default
language; unknown keys and changed placeholder names are rejected. English is the
default when present; otherwise the first sorted language becomes the default.
Removing all but one language file and regenerating hides the selector.
New languages increase firmware size; keep the selected application/OTA partition
large enough.

`ui_*` keys are rendered with DOM `textContent` / placeholders, not injected as
HTML. `s_*` keys are used by native messages. To add a description, add its
key to the default catalog, provide translations and reference it from the code.
Protocol routes, JSON field names, MQTT topics and device identity are not
translated. Runtime JSON syntax diagnostics remain stable English error codes.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
