# Dependent fields and pages

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## `visible` and `visibleWhen`

`visible` is a boolean initial flag changed at runtime with `setFieldVisible()`
or `setPageVisible()`. It requires `ARDPORTAL_ENABLE_VISIBILITY` (default `1`).
`visibleWhen` is a condition based on registered field values and requires
`ARDPORTAL_ENABLE_DEPENDENCIES`. Both can be used together: the field flag,
page flag and dependency rules must all allow visibility. A runtime `true` flag
cannot override a false condition. Hiding a referenced field also makes a
condition referring to it false, even if its stored value still matches.

Navigation groups automatically hide when no member page is visible. Apply
visibility to member pages, not to the group definition.
See [Programmatic visibility](https://github.com/rkubera/ArdPortal/wiki/Dynamic-Pages#programmatic-visibility)
and [Defines](https://github.com/rkubera/ArdPortal/wiki/Defines#programmatic-visibility).

## One field definition, multiple conditions

Each field ID is globally unique. Define D once and put its visibility expression
on that definition. D can depend on B OR C, B AND C, or nested combinations;
registering D twice is rejected rather than creating two linked copies.

| `visibleWhen` property | Required | Meaning |
| --- | --- | --- |
| `field` | Leaf only | ID of a registered field on this page or an earlier registered page; arbitrary unregistered App Config keys cannot control visibility. |
| `property` | No | One direct property of a composite field, such as `state`; dotted/nested paths are not supported. |
| `equals` | Leaf only | Exact scalar JSON comparison: string, boolean, number or null. Objects/arrays are rejected. String `"1"` differs from number `1`. Numeric comparison uses serialized
JSON text too, so lexemes such as `1` and `1.0` can differ. |

A condition is either a leaf (`field`, `equals`, optional `property`) or a
single `and`/`or` group containing a nonempty array of conditions. Groups can nest:
`and` requires every child; `or` requires at least one. A group cannot also contain
leaf properties, and a condition cannot contain both `and` and `or`. There is no
`not`, `enabledWhen` or expression language.

Each field/page condition supports up to 6 nested groups and 64 total condition
nodes, including groups and leaves. These bounds protect the device stack and
loop work. Page and field conditions are still combined with AND. A leaf whose
referenced field is hidden evaluates false, even if its saved value matches;
another true OR branch can still make the dependent field visible.

Rules never erase stored values. Missing references and dependency cycles are
rejected during registration, even when a cyclic branch is currently false or
another OR branch is true. Fields in the same registration job or earlier
registered pages/entities can be referenced. Rejected page jobs remain atomic;
no partial fields or HA entities are exposed.

Dependency support defaults to enabled. Set
`#define ARDPORTAL_ENABLE_DEPENDENCIES 0` before including `ArdPortal.h` to
compile it out, including browser logic and its native message. Dynamic pages
and ordinary fields remain supported. Registration rejects definitions containing
`visibleWhen` when this flag is zero; it does not silently ignore their rules.
Set `ARDPORTAL_ENABLE_DEPENDENCIES=0` before including `ArdPortal.h` to exclude dependency handling.

Fields can declare a `visibleWhen` dependency:

```json
{
  "id": "speed", "type": "slider", "name": "Speed", "min": 0, "max": 100,
  "visibleWhen": {"field": "fan", "property": "state", "equals": "ON"}
}
```

The original single-field syntax remains supported. To show D when A is true
and either B or C is true, define D once and use a grouped condition:

```json
{
  "id": "D", "type": "edit", "name": "D",
  "visibleWhen": {
    "and": [
      {"field": "A", "equals": true},
      {"or": [
        {"field": "B", "equals": true},
        {"field": "C", "equals": true}
      ]}
    ]
  }
}
```

A, B and C must be registered fields (or fields in the same page job). Leaves
inside any group can use `property` for composite values. The same evaluator
controls portal visibility, HTTP/MQTT command acceptance and HA discovery.
HA processes dependency edges incrementally; registration cycle checks also
advance within the configured registration budget. No extra define is needed.

Dependency traversal starts with at most eight 4-byte frames and grows only when
an ancestor chain needs more space, up to the registered field count. Short
chains with 1024 registered fields therefore start at 32 bytes rather than
reserving 4096 bytes per traversal. During buffer growth, old and new arrays
briefly coexist; allocation failure preserves the old array and fails safely.
Registration cycle checks reuse their completed-node and pending-root masks.
Boolean evaluation and schema validation share non-templated traversal code;
callback adapters borrow their context without allocating `std::function` objects.

Entire pages can use the same simple or grouped conditions, referencing a field
on a previously registered page:

```json
{
  "id": "advanced", "name": "Advanced", "order": 20,
  "visibleWhen": {"field": "advanced_enabled", "equals": true},
  "fields": [{"id": "note", "type": "edit", "name": "Note"}]
}
```

When false, the page's menu link and content are hidden, including a Home block.
All its fields inherit the condition, including HA discovery and command checks.
A field with its own rule is visible only when both rules match. Values remain
stored. Keep the controlling field on an accessible page so users can re-enable
it. Page visibility also refreshes on built-in portal pages.

The rule uses an exact scalar comparison (string, number, boolean or null).
`property` optionally selects one property of a composite field. Referenced fields
must belong to the same page or a previously registered page. Rules compare reported
values and also require the referenced field to be visible, including its page
and all upstream field/page conditions. Missing properties do not match. Omit
the rule to keep a field visible (subject to its page condition).

Dependencies cascade: if page A is hidden, a page B controlled by a field on A
is also hidden, even when that field retains a matching value. Its HA entities
follow the same cascade. Cycles are rejected at registration; a failed runtime evaluation does not
erase stored values. Keep the root controlling field outside the dependent
pages. Evaluation uses a bounded iterative graph walk with yields, without
recursive calls or retaining parsed ancestor definitions.

For example, register these pages in the order shown. The root switch is always
accessible. Advanced contains another switch controlling Details:

```cpp
static const char ROOT_PAGE[] PROGMEM = R"JSON({
  "id": "settings", "name": "Settings", "order": 10,
  "fields": [{"id": "advanced_enabled", "type": "switch",
              "name": "Advanced settings", "default": false}]
})JSON";

static const char ADVANCED_PAGE[] PROGMEM = R"JSON({
  "id": "advanced", "name": "Advanced", "order": 20,
  "visibleWhen": {"field": "advanced_enabled", "equals": true},
  "fields": [{"id": "details_enabled", "type": "switch",
              "name": "Show details", "default": true}]
})JSON";

static const char DETAILS_PAGE[] PROGMEM = R"JSON({
  "id": "details", "name": "Details", "order": 30,
  "visibleWhen": {"field": "details_enabled", "equals": true},
  "fields": [{"id": "details_note", "type": "edit",
              "name": "Note", "default": ""}]
})JSON";

// In setup(), before portal.begin(): register in dependency order.
const __FlashStringHelper* definitions[] = {
  FPSTR(ROOT_PAGE), FPSTR(ADVANCED_PAGE), FPSTR(DETAILS_PAGE)
};
portal.onAppConfigPageRegistrationFinished([definitions, next = size_t(1)](bool success) mutable {
  if (!success) {
    Serial.println(portal.appConfigRegistrationError().reason);
    return;
  }
  if (next < 3 && !portal.startAppConfigPageRegistration(definitions[next++]))
    Serial.println(portal.appConfigRegistrationError().reason);
});
if (!portal.startAppConfigPageRegistration(definitions[0]))
  Serial.println(portal.appConfigRegistrationError().reason);
```

| Advanced enabled | Details enabled (stored) | Visible pages |
|---|---|---|
| `false` | Either value | Settings |
| `true` | `false` | Settings, Advanced |
| `true` | `true` | Settings, Advanced, Details |

Turning Advanced off hides Details even if `details_enabled` is still `true`.
Turning it back on restores both pages and their existing values. Field-only
chains work the same way: a dependency on a hidden field fails even if that
field's stored value matches `equals`.

Do not make a page depend on one of its own fields: that field inherits the
page condition and creates a cycle. Likewise, mutually dependent fields and
self-referencing fields are rejected at registration with `dependency cycle`.
This check includes every AND/OR branch regardless of current values. A shared upstream
controller used by several branches is allowed and is not a cycle.

The existing `ARDPORTAL_ENABLE_DEPENDENCIES` flag controls field rules, page rules
and cascades together; no additional define is required. With HA enabled, hiding
Advanced also removes the retained discovery configurations for its fields and
all hidden descendants. Showing it again restores the eligible entities and
publishes their current values. With HA disabled, portal visibility and command
checks still work.

The portal hides the field when the rule fails. Changes from the program, MQTT
and portal refresh visibility through WebSocket revision notifications. Commands
to hidden fields are rejected by the device, including MQTT commands;
programmatic state/config updates remain allowed. Stored values are preserved.

In HA, a false rule publishes an empty retained discovery payload; a true rule
republishes discovery and the current state. Reconnect, HA birth and periodic
discovery follow the same rules. This removes/recreates entities rather than
configuring dashboard visibility: manually configured cards can show a
missing-entity warning.

The main sketch includes a Dependencies page with a switch controlling visibility
of a level slider and a note input. A minimal standalone version is available in
`examples/DynamicPagesWithDependencies/DynamicPagesWithDependencies.ino`.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)


## Zigbee

Field and page dependency rules do not affect control visibility in Zigbee.
All registered supported endpoints remain available in the gateway, even when
their fields are hidden in ArdPortal. Zigbee commands remain accepted for these
fields, including compound controls and actions, with normal value validation
and read-only restrictions. Portal and MQTT/HA dependency behavior is unchanged.
