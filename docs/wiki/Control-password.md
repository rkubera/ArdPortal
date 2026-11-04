# password control

[Control catalogue](https://github.com/rkubera/ArdPortal/wiki/Controls) · [Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

```json
{
  "id": "api_password",
  "type": "password",
  "name": "API password",
  "maxLength": 1024,
  "persist": true
}
```

The portal uses a password input: typed characters are hidden. Changes are sent
when the input changes and loses focus, rather than after each typing pause.
An empty portal input preserves the existing value, as with the Wi-Fi password.
After a successful update the input is cleared. Stored passwords and password
`default` values are never returned by the normal page/value HTTP endpoints.

`id`, `type` and `name` (or `names`) follow the shared control rules. `maxLength`
is optional and accepts 1–8192 UTF-8 bytes, default 128. Only strings without
embedded NUL are accepted. `default` is optional (default `""`), and `persist`
defaults to true; false keeps changes in RAM. The application's normal getter and
setter access the real value. Use the application setter to explicitly clear it.
Feature flag: `ARDPORTAL_ENABLE_CONTROL_PASSWORD` (defaults to the controls flag).

This is a local basic control. It creates no Home Assistant, Matter or Zigbee
entity and has no MQTT state/command route, even with `ha` or `mqtt` overrides.
Semantic classes and custom extended `controls` are not supported.

Backup masks saved password values with the same reversible UTF-8 transform as
the portal Wi-Fi password. A `maskedAppPasswords` list identifies transformed
application keys; restore reverses the mask before normal validation and saving.
Older backups without this list retain their original application values.
The list makes restore independent of page-registration timing. Only persisted
values are backed up; runtime-only values and firmware defaults are not exported.

As with Wi-Fi credentials, the ordinary flash configuration stores the real
value. Backup masking is obfuscation, not encryption; normal portal updates use
the same HTTP transport as other settings.
