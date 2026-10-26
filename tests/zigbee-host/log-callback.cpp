#define ARDPORTAL_ZIGBEE_ROUTER 0
#define ARDPORTAL_ENABLE_LOG_CALLBACK 1
#define ARDPORTAL_ENABLE_WWW 0
#include "ArdZigbeeImpl.h"
#include <cassert>
#include <vector>
uint32_t fakeMillis = 0;
time_t fakeUnixTime = 0;
static void sink(void* context, const String& text) {
  static_cast<std::vector<String>*>(context)->push_back(text);
}
int main() {
  ArdZigbee module;
  std::vector<String> messages;
  module.onLogDiagnostic(&messages, sink);
  module.diagnostic("test event");
  assert(messages.size() == 1 && messages.back() == "test event");
  assert(module.nextLine(0) == nullptr);
  String longText;
  for (unsigned i = 0; i < 300; ++i) longText += 'x';
  module.diagnostic(longText);
  assert(messages.size() == 2 && messages.back().length() == 300);
  module.onLogDiagnostic(nullptr, nullptr);
  module.diagnostic("detached");
  assert(messages.size() == 2);
}
