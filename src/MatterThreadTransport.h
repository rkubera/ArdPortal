// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <esp_netif_net_stack.h>
#include <esp_openthread_netif_glue.h>
#include <lwip/tcpip.h>
#include <lwip/udp.h>

namespace ArdMatterThreadTransport {
// ESP-IDF's socket sendmsg does not apply IPv6 packet-info interface selection.
// Bind only Matter's IPv6 UDP PCB; leave portal sockets and default routes alone.
/** @brief Bind existing Matter IPv6 UDP sockets on port to Thread under the TCP/IP lock; false if unavailable or no PCB matched. */
inline bool bind(uint16_t port) {
  auto* thread = esp_openthread_get_netif();
  if (!thread || !port) return false;
  auto* interface = static_cast<netif*>(esp_netif_get_netif_impl(thread));
  if (!interface) return false;
  struct Request {
    netif* interface;
    uint16_t port;
    bool found;
  } request{ interface, port, false };
  const err_t result = tcpip_callback_wait([](void* context) {
    auto& request = *static_cast<Request*>(context);
    for (auto* pcb = udp_pcbs; pcb; pcb = pcb->next) {
      if (pcb->local_port == request.port && IP_IS_V6(&pcb->local_ip)) {
        udp_bind_netif(pcb, request.interface);
        request.found = true;
      }
    }
  },
                                           &request);
  return result == ERR_OK && request.found;
}
}
