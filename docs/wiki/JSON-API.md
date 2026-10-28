# ArdJSON API

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## What is ArdJSON?

**ArdJSON is the JSON parser, value container and serializer included in ArdPortal.** JSON is a text format for structured data: objects contain named properties, arrays contain ordered values, and leaves contain strings, numbers, booleans or null. For example, `{"enabled":true,"temperature":22}` is a JSON object with two properties.

ArdJSON converts between that text and C++ values:

- **Parsing:** `JSON.parse(text)` checks JSON syntax and creates a `JSONVar` tree in RAM.
- **Working with data:** `JSONVar` represents an object, array or scalar. Use property/index access and checked conversions to inspect or change values.
- **Serialization:** `JSON.stringify(value)` converts the tree back to JSON text. `measure()` and `stringifySlice()` also support bounded output without allocating one complete output string.

ArdPortal uses ArdJSON to interpret dynamic page definitions, validate configuration and command values, and produce HTTP, MQTT and saved configuration JSON. Applications can also use it independently through `#include <ArdJSON.h>`; no portal, network connection or mounted filesystem is required. See the [JsonBasics example](https://github.com/rkubera/ArdPortal/blob/main/examples/JsonBasics/JsonBasics.ino).

ArdJSON does **not** persist values. Its trees occupy RAM and disappear on restart. Use [ArdFS](https://github.com/rkubera/ArdPortal/wiki/FS-API) to save serialized JSON, or the [Portal AppConfig API](https://github.com/rkubera/ArdPortal/wiki/Portal-API) for integrated configuration storage. Parsing a large document can still exhaust RAM: input length, node count, string length and nesting limits constrain different parts of that cost. Check returned values and error messages before using the result.

## Standalone JSON API

`ArdJSON.h` is independent of the portal/filesystem and uses only Arduino/STL.
Parsing shares the streaming syntax engine used by ArdFS. Arduino and the supplied CMake target compile the implementation automatically. Custom native builds must also compile `src/JsonStream.cpp` and `src/JsonDocumentStream.cpp`; including the header alone is sufficient only for value/serialization operations.

It provides `JSON.parse`, `JSON.stringify`, `JSON.typeof_`
and `JSONVar` values:

```cpp
#include <ArdJSON.h>
using ArdJSON::JSON;
using ArdJSON::JSONVar;

JSONVar settings = JSONVar::object();
settings["name"] = "ArdUI";
settings["enabled"] = true;
settings["values"] = JSONVar::array();
settings["values"].push(42);

String error;
String text = JSON.stringify(settings, true, &error);
const JSONVar parsed = JSON.parse(text, &error);
if (parsed.isValid()) {
  String name = parsed["name"].asString();
}
```

Supported types: undefined, null, boolean, number, string, object and array.
Use `hasOwnProperty`, `keys`, `length`, `push`, `remove`, `asString`, `asBool`,
`asDouble`, `toInteger`, `toUnsignedInteger` and `toDouble` for inspection/mutation. Missing members
are undefined. Values use deep copies. Numbers preserve their original lexeme;
use checked integer conversion when exact large integers matter.

Parsing validates UTF-8, escapes, Unicode surrogate pairs, JSON number syntax,
duplicate keys and trailing content. General JSON strings support U+0000;
portal settings reject it because network APIs consume C strings.
`JSON.measure(value)` returns the encoded byte count without allocating the full
output. `JSON.stringifySlice(value, offset, maximum)` serializes a bounded slice.
Set `Limits.escapeHtml = true` when embedding JSON in HTML to escape `<`, `>`
and `&`. The portal uses these methods to stream page definitions in 256-byte
chunks. Serialization work within each chunk remains synchronous.

`ArdJSON::Limits` can bound input/output, strings, nodes and depth. Defaults are
32768 input/output bytes, 8192 string bytes, 256 nodes and depth 16; hard depth
limit is 32 and a container may have at most 1024 elements. Parsing, serializing,
CRC and memory allocation are synchronous CPU operations.

### ArdJSON method reference and checked access

| API | Result / behavior |
| --- | --- |
| `JSON.parse(text, error=nullptr, limits=Limits())` | JSONVar; `isValid()==false` on syntax/resource failure. Error includes the byte offset. |
| `JSON.stringify(value, pretty=false, error=nullptr, limits=Limits())` | Encoded String; empty on failure. Undefined is not a JSON value. |
| `JSON.measure(value, error=nullptr, limits=Limits())` | Compact encoded byte count, or zero on failure; no full output String. |
| `JSON.stringifySlice(value, offset, maximum, error=nullptr, limits=Limits())` | Slice of compact serialized JSON; an empty slice can also mean end of output. Does not consume or mutate the value. |
| `JSON.typeof_(value)` | C-string type name: undefined/null/boolean/number/string/object/array. |
| `JSONVar()` / `JSONVar(nullptr)` | Undefined / JSON null; these are different values. |
| `JSONVar::object()` / `JSONVar::array()` | Empty object / array. |
| `type()`, `isUndefined()`, `isNull()`, `isValid()` | Type checks; isValid checks the complete tree, including allocation failures and Undefined children. |
| `operator[](key)` / `operator[](index)` | Mutable element reference or const access; prefer const access when inspecting to avoid accidental insertion. Missing mutable keys can create Undefined members. |
| `hasOwnProperty(key)` / `keys()` | Test an object key / copy its keys as a JSON array. |
| `length()` | Number of container members/elements; not the byte length of a string. |
| `push(value)` / `tryPush(value)` | Append to an array; bool reports success. `tryPush` preserves the array on allocation failure. |
| `remove(key)` / `remove(size_t index)` | Remove object member / array element; returns success. |
| `stringValue()` | Borrow a `const String*` without copying; nullptr for non-string values. Valid until the JSON value is changed or destroyed. |
| `asString()` / `asBool()` / `asDouble()` | Convenience accessors: wrong types produce empty/false/zero; not validation. |
| `isInteger()` / `toInteger(int64_t&)` | Integer-lexeme test / checked signed conversion with range validation. |
| `toUnsignedInteger(uint64_t&)` / `toDouble(double&)` | Checked numeric conversion; return false for incompatible/invalid/out-of-range input. |
| Copy/move/assignment, `swap(other)` | Deep-copy ownership or move/swap; no shared mutable DOM nodes. |

For settings and commands, validate types and ranges before using values. Do not
interpret `asDouble()==0` as proof that a missing/malformed field really is zero:

```cpp
// README example: checked JSON parsing and application bounds.
ArdJSON::Limits limits;
limits.maxInputBytes = 512;
limits.maxOutputBytes = 512;
limits.maxStringBytes = 128;
limits.maxNodes = 16;
limits.maxDepth = 3;
String error;
const auto input = ArdJSON::JSON.parse("{\"interval\":60}", &error, limits);
int64_t interval = 0;
if (input.isValid() && input.type() == ArdJSON::JSONVar::Type::Object &&
    input["interval"].toInteger(interval) && interval >= 10 && interval <= 3600) {
  // Use interval; it has been validated for this application.
}
String encoded = ArdJSON::JSON.stringify(input, false, &error, limits);
if (!encoded.length()) Serial.println(error);
```

`Limits` fields are `maxInputBytes`, `maxOutputBytes`, `maxStringBytes`, `maxNodes`,
`maxDepth` and `escapeHtml`; the defaults are listed above. Limits are per
parse/write operation; raising them is not a preallocation or a guarantee of RAM.
`JSONVar` mutations also need heap and can invalidate a value, so check `push()`
and `isValid()` before saving/publishing a constructed document. Decimal JSON
lexemes are retained exactly on parse; constructing from a floating-point number
can expose binary rounding. Format decimal strings explicitly if your wire format
requires a fixed number of decimal places.

`ArdJsonCodec` in `JsonCodec.h` delegates `parse(input, error, limits)` and
`stringify(value, pretty, error, limits)` to ArdJSON. It introduces no additional
backend or independent memory pool. The portal/ArdFS call it internally; standalone
code can call `ArdJSON::JSON` directly.

## Object iteration and patch API

These APIs support changing a few application keys without cloning the whole
configuration. A patch is an object; its top-level members replace whole values,
not a recursive merge of nested objects.

| API | Inputs and result |
| --- | --- |
| `forEachObjectMember(visitor)` | Calls `bool visitor(const String& key, const JSONVar& value)` for each member, without allocating a complete keys array. Returns false for a nonobject/failure or when the visitor stops. Borrowed references expire with the call. |
| `isObjectPatchValid()` | True for an object whose members are valid values or Undefined deletion markers. Ordinary `isValid()` rejects Undefined children. |
| `applyObjectPatch(JSONVar&& patch, bool eraseUndefined=true)` | Validates object base/patch and moves member ownership without deep copies. Replaces existing members, adds new ones, and by default removes keys marked Undefined. `eraseUndefined=false` retains Undefined values rather than deleting keys; such an object is not an ordinary valid JSON document. Returns false for invalid inputs/self-patching/element limit; patch is consumed on success. |
| `JSON.measureObjectPatch(base, patch, error, limits)` | Counts compact bytes of the overlaid object without changing either input; zero on failure. Undefined patch members omit keys. |
| `JSON.stringifyObjectPatchSlice(base, patch, offset, maximum, error, limits)` | Serializes the requested compact byte slice of the overlaid object without cloning it. Empty can mean failure or end of output; inspect error/measure. |

```cpp
using ArdJSON::JSONVar;
JSONVar settings = JSONVar::object();
settings["period"] = 60;
settings["old"] = true;
JSONVar patch = JSONVar::object();
patch["period"] = 120;
patch["old"] = JSONVar(); // Undefined is a deletion marker in an object patch.
if (patch.isObjectPatchValid() && settings.applyObjectPatch(std::move(patch))) {
  String error;
  String text = ArdJSON::JSON.stringify(settings, false, &error);
  // text is {"period":120}; check error before persisting/publishing.
}
```

`Parser`, `Writer` and `ObjectKey` are implementation classes. Prefer the public
`JSON` facade and `JSONVar` APIs. Allocation failure does not mean a numeric value
of zero or an empty valid object: check Boolean results and `isValid()`.

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
