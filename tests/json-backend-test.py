"""Check optional JSON selection and run conformance against real ArduinoJson 7.

Set ARDUINOJSON_ROOT to the installed library directory to test the optional backend.
No third-party library is required for the default backend.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
flags = [os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-Wall', '-Wextra', '-Werror',
         '-fsanitize=address,undefined', '-I' + str(root / 'tests/memory-host'), '-I' + str(root / 'src')]
sources = [str(root / 'src' / name) for name in ('JsonStream.cpp', 'JsonDocumentStream.cpp', 'JournalReader.cpp')]
probe = r'''
#include "ArdPortalFeatures.h"
#include "ArdJSON.h"
#include <cassert>
#include <type_traits>
uint32_t fakeMillis = 0;
static_assert(bool(ARDPORTAL_FEATURE_MASK & (uint64_t(1) << 55)) == bool(ARDPORTAL_USE_ARDUINOJSON), "signature");
#if ARDPORTAL_USE_ARDUINOJSON
static_assert(std::is_same<ArdJSON::Parser, ArdJSON::ArduinoParser>::value, "parser");
static_assert(std::is_same<ArdJSON::StreamParser, ArdJSON::ArduinoStreamParser>::value, "storage parser");
#else
static_assert(std::is_same<ArdJSON::Parser, ArdJSON::NativeParser>::value, "parser");
#endif
int main() {
  const String input = "{\"a\\u0000b\":\"x\\u0000y\",\"array\":[null,true,-0,1.2300e+4,18446744073709551616],\"html\":\"</script>&\"}";
  auto value = ArdJSON::JSON.parse(input);
  assert(value.isValid());
  for (bool pretty : {false, true}) for (bool html : {false, true}) {
    ArdJSON::Limits limits; limits.escapeHtml = html;
    String error;
    String expected = ArdJSON::NativeAPI().stringify(value, pretty, nullptr, limits);
    String actual = ArdJSON::JSON.stringify(value, pretty, &error, limits);
    assert(actual == expected && !error.length());
    limits.maxOutputBytes = actual.length();
    assert(ArdJSON::JSON.stringify(value, pretty, &error, limits) == actual && !error.length());
    --limits.maxOutputBytes;
    assert(!ArdJSON::JSON.stringify(value, pretty, &error, limits).length() && error.length());
  }
}
'''
with tempfile.TemporaryDirectory(prefix='ardportal-json-test-') as temporary:
    temporary = Path(temporary)
    source = temporary / 'backend.cpp'
    source.write_text(probe)
    dependency = os.environ.get('ARDUINOJSON_ROOT')
    backends = [0, 1] if dependency else [0]
    for backend in backends:
        options = [f'-DARDPORTAL_USE_ARDUINOJSON={backend}']
        if backend:
            options += ['-I' + str(Path(dependency) / 'src'), '-DARDUINOJSON_ENABLE_ARDUINO_STRING=0',
                        '-DARDUINOJSON_ENABLE_PROGMEM=0']
        dependencies = subprocess.check_output(flags + options + ['-MM', str(source)], text=True)
        assert ('ArduinoJson.h' in dependencies) == bool(backend)
        for name in ('backend', 'json-numbers', 'json-stream', 'json-patch'):
            test = source if name == 'backend' else root / 'tests/memory-host' / (name + '.cpp')
            executable = temporary / f'{name}-{backend}'
            subprocess.run(flags + options + [str(test)] + sources + ['-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
    invalid = subprocess.run(flags + ['-DARDPORTAL_USE_ARDUINOJSON=2', '-fsyntax-only', str(source)], capture_output=True, text=True)
    assert invalid.returncode and 'must be 0 or 1' in invalid.stderr
print('PASS: JSON selection, dependency exclusion, feature signature and conformance')
if not dependency:
    print('Optional ArduinoJson checks require ARDUINOJSON_ROOT pointing to ArduinoJson 7')
