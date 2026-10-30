#pragma once
#include <Client.h>
// Only used to test header selection and the transport observer. Actual
// PubSubClient compatibility is checked by the Arduino firmware builds.
class PubSubClient {
public:
  void setClient(Client&) {}
};
