// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#if defined(ESP32)
#include <lwip/sockets.h>
#include <errno.h>
#endif
namespace ArdNetworkIo {
/**
 * @brief Write only the currently available TCP capacity without waiting for the peer.
 * @param client TCP client used for the transfer.
 * @param data Data buffer or value used by the operation.
 * @param length Number of bytes or elements to process.
 * @return Number of bytes written immediately; zero when the socket cannot currently accept data.
 */
inline size_t writeChunk(WiFiClient& client, const uint8_t* data, size_t length) {
#if defined(ESP32)
  // Bypass WiFiClient::write's internal select/retry loop.
  int result = ::send(client.fd(), data, length, MSG_DONTWAIT);
  if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) client.stop();
  return result > 0 ? size_t(result) : 0;
#else
  int room = client.availableForWrite();
  if (room <= 0) return 0;
  if (length > size_t(room)) length = room;
  return client.write(data, length);
#endif
}
/**
 * @brief Close the TCP client; abort on ESP8266 to avoid waiting for a graceful shutdown.
 * @param client TCP client used for the transfer.
 * @return No value.
 */
inline void stopClient(WiFiClient& client) {
#if defined(ESP8266)
  client.abort(); // Avoid ESP8266 stop() waiting for graceful TCP shutdown.
#else
  client.stop();
#endif
}
}
