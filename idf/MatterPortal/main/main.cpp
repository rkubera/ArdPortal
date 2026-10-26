// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
// The same control definitions and callbacks in both build systems.
#if ARDPORTAL_SDK_DIAGNOSTICS
/** @brief Install bounded SDK logging when the diagnostic build is selected. */
void ardportalSdkDiagnosticsBegin();
/** @brief Forward queued SDK records from the application loop. */
void ardportalSdkDiagnosticsDrain();
#endif
#if ARDPORTAL_MATTER_CONTROL_VALIDATION
#include "../../../tests/device/MatterControls.ino"
#else
#include "../../../examples/MatterPortal/MatterPortal.ino"
#endif
#if ARDPORTAL_SDK_DIAGNOSTICS
#include "SdkDiagnostics.h"
#include <lwip/ip6.h>
extern "C" struct netif* __real_ip6_route(const ip6_addr_t* source, const ip6_addr_t* destination);
/** @brief Trace IPv6 interface selection and return the original SDK routing result unchanged. */
extern "C" struct netif* __wrap_ip6_route(const ip6_addr_t* source, const ip6_addr_t* destination) {
  auto* interface = __real_ip6_route(source, destination);
  char from[48], to[48];
  ip6addr_ntoa_r(source, from, sizeof(from));
  ip6addr_ntoa_r(destination, to, sizeof(to));
  esp_log_write(ESP_LOG_INFO, "AP-ROUTE", "AP-ROUTE: %s -> %s via %c%c%u\n", from, to,
                interface ? interface->name[0] : '-', interface ? interface->name[1] : '-',
                interface ? interface->num : 0);
  return interface;
}
void ardportalSdkDiagnosticsBegin() {
  ArdSdkDiagnostics::begin();
}
void ardportalSdkDiagnosticsDrain() {
  ArdSdkDiagnostics::drain(portal);
}
#endif
