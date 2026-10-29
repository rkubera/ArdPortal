#include <cassert>
#define ARDPORTAL_TEST_ALLOCATE testAllocate
#include "ArdJSON.h"
long allocationBudget = -1;
size_t allocations = 0;
void* testAllocate(size_t bytes) noexcept {
  if (allocationBudget == 0) return nullptr;
  if (allocationBudget > 0) --allocationBudget;
  ++allocations;
  return std::malloc(bytes);
}
uint32_t fakeMillis = 0;
int main() {
  using namespace ArdJSON;
  assert(JSON.parse("true").asBool());
  assert(JSON.parse("false").type() == JSONVar::Type::Boolean && !JSON.parse("false").asBool());
  assert(JSON.parse("null").isNull());
  for (const char* literal : { "t", "tr", "tru", "f", "fa", "fal", "fals", "n", "nu", "nul", "trux", "truefalse", "false true" })
    assert(JSON.parse(literal).isUndefined());
  String embeddedLiteral = "tr";
  embeddedLiteral += char(0);
  embeddedLiteral += 'e';
  assert(JSON.parse(embeddedLiteral).isUndefined());
  JSONVar base = JSONVar::object(), patch = JSONVar::object();
  for (int i = 0; i < 300; ++i) base[String("field") + String(i)] = i;
  patch["field3"] = 99;
  patch["field4"] = JSONVar();
  patch["new"] = true;
  Limits l;
  l.maxNodes = 4096;
  size_t n = JSON.measureObjectPatch(base, patch, nullptr, l);
  assert(n > 0);
  String text;
  for (size_t p = 0; p < n; p += 31) text += JSON.stringifyObjectPatchSlice(base, patch, p, 31, nullptr, l);
  auto merged = JSON.parse(text, nullptr, l);
  assert(merged.isValid() && merged.length() == 300 && merged["field3"].asDouble() == 99 && !merged.hasOwnProperty("field4"));
  allocationBudget = 0;
  assert(base.applyObjectPatch(std::move(patch)));
  allocationBudget = -1;
  assert(JSON.stringify(base, false, nullptr, l) == text);
  // A staged deletion must survive later edits before commit.
  JSONVar pending = JSONVar::object(), del = JSONVar::object();
  del["field3"] = JSONVar();
  assert(pending.applyObjectPatch(std::move(del), false));
  assert(pending.length() == 1 && pending.isObjectPatchValid());
  JSONVar add = JSONVar::object();
  add["field5"] = 42;
  assert(pending.applyObjectPatch(std::move(add), false));
  assert(base.applyObjectPatch(std::move(pending)));
  assert(!base.hasOwnProperty("field3") && base["field5"].asDouble() == 42);
  // Capacity overflow is rejected by both the writer and atomic commit.
  JSONVar full = JSONVar::object();
  for (unsigned i = 0; i < MaxContainerElements; ++i) full[String(i)] = 0;
  JSONVar overflow = JSONVar::object();
  overflow["extra"] = true;
  l.maxNodes = 4096;
  assert(!JSON.measureObjectPatch(full, overflow, nullptr, l));
  assert(!full.applyObjectPatch(std::move(overflow)));
  assert(full.length() == MaxContainerElements && overflow.length() == 1);
  // An allocation-failed patch cannot corrupt its destination.
  JSONVar invalid = JSONVar::object();
  allocationBudget = 0;
  invalid["new"] = 7;
  assert(!full.applyObjectPatch(std::move(invalid)));
  allocationBudget = -1;
  assert(full.isValid());

  // Shared lookup must preserve exact keys across RAM/flash storage and NULs.
  String binaryKey = "name";
  binaryKey += char(0);
  binaryKey += "suffix";
  const String longKey = "application-owned-key-longer-than-small-string-storage";
  JSONVar exact = JSONVar::object();
  exact["name"] = 1;
  exact[String()] = 2;
  exact[binaryKey] = 3;
  exact[longKey] = 4;
  const JSONVar& readOnly = exact;
  allocationBudget = 0;
  assert(readOnly.hasOwnProperty(String("name")) && readOnly["name"].asDouble() == 1);
  assert(readOnly.hasOwnProperty(String()) && readOnly[String()].asDouble() == 2);
  assert(readOnly.hasOwnProperty(binaryKey) && readOnly[binaryKey].asDouble() == 3);
  assert(readOnly.hasOwnProperty(longKey) && readOnly[longKey].asDouble() == 4);
  assert(readOnly[String("missing")].isUndefined());
  assert(exact[binaryKey].asDouble() == 3 && exact.length() == 4);
  allocationBudget = -1;
  JSONVar exactPatch = JSONVar::object();
  exactPatch["name"] = 5;
  exactPatch[binaryKey] = JSONVar();
  exactPatch[longKey] = 6;
  exactPatch["added"] = true;
  JSONVar applied = exact;
  JSONVar copiedPatch = exactPatch;
  assert(applied.applyObjectPatch(std::move(copiedPatch)));
  const String expected = JSON.stringify(applied);
  const size_t measured = JSON.measureObjectPatch(exact, exactPatch);
  assert(measured == expected.length());
  String slices;
  for (size_t at = 0; at < measured; ++at)
    slices += JSON.stringifyObjectPatchSlice(exact, exactPatch, at, 1);
  assert(slices == expected && exact.length() == 4 && exactPatch.length() == 4);

  JSONVar removeObject = JSON.parse("{\"first\":{\"nested\":[1,2]},\"middle\":true,\"last\":[3]}");
  JSONVar removeArray = JSON.parse("[{\"key\":[1,2]},3,4]");
  assert(removeObject.isValid() && removeArray.isValid());
  allocationBudget = 0;
  assert(removeObject.remove(String("middle")) && removeObject.length() == 2);
  assert(removeObject.remove(String("first")) && removeObject.length() == 1);
  assert(removeObject.remove(String("last")) && removeObject.length() == 0);
  assert(!removeObject.remove(String("missing")) && removeObject.isValid());
  assert(removeArray.remove(size_t(1)) && removeArray.length() == 2);
  assert(removeArray[size_t(1)].asDouble() == 4);
  assert(removeArray.remove(size_t(0)) && removeArray.length() == 1);
  assert(removeArray.remove(size_t(0)) && removeArray.length() == 0);
  assert(!removeArray.remove(size_t(0)) && removeArray.isValid());
  allocationBudget = -1;
}
