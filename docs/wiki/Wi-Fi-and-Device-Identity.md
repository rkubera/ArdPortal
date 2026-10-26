# Wi-Fi, AP and device identity

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## Portal pages and device identity

| Page | URL | Features |
| --- | --- | --- |
| Home | `/` | Device information; navigation is in the header |
| Device | `/device` | Device name and factory reset |
| Wi-Fi configuration | `/wifi` | Background scan, credentials, clear saved Wi-Fi |
| AP configuration | `/ap` | AP password and current network name |
| MQTT configuration | `/mqtt` | Broker, port, credentials, optional TLS and CA |
| Update | `/upgrade` | Streaming application firmware OTA |
| Console | `/console` | MQTT and diagnostic messages over WebSocket |

Each settings page has a Cancel button returning to Home. The CA field is hidden
when TLS is disabled. Home information includes chip type/ID, configured and
physical flash, heap, sketch/OTA space, uptime, NTP date/time, connections and IPs.

The device name is shared by the AP SSID, station hostname, MQTT client ID and
browser page title. The default is `ArdUI-<chip ID>`. It accepts 1–32 ASCII
letters, digits and hyphens, without a leading/trailing hyphen. Change it on the
Device page; AP configuration shows it as read-only. Saving identity changes
reconnects the device after the HTTP response has been sent.

AP passwords accept 8–63 bytes. An empty password field preserves the saved
password. Wi-Fi and MQTT password fields similarly preserve the password when
the network or broker/user is unchanged. An empty broker disables MQTT; an
empty SSID disables station connection attempts. Secret fields are not returned
by the HTTP configuration API.

## Wi-Fi, AP fallback and captive portal

The device first tries the saved Wi-Fi network for `wifiTimeoutMs` (30 seconds
by default). If it cannot connect in that time, it starts its AP. After losing
an established connection, it starts a fresh timed attempt and falls back to the
AP again if necessary. Retries are separated by `retryMs`. Automatic connection
attempts pause while any station is connected to the fallback AP, preserving
portal access when the saved network is unavailable. They resume after the last
station disconnects. An explicit Wi-Fi form submission still starts its requested
connection attempt.

The HTTP server remains running after reboot and while Wi-Fi/MQTT are connected.
Once Wi-Fi connects, the AP closes when it has no stations and no active HTTP
response. Continue using the LAN IP shown by the portal. A failed MQTT
connection does not trigger AP fallback.

The Wi-Fi connection result message is shown only for a submitted Wi-Fi form,
not merely on entering Home or following an automatic reconnect. A successful
attempt shows the LAN URL. A timeout reports failure and keeps the Wi-Fi form
available. Clearing Wi-Fi removes only the SSID/password, preserves the other
settings and starts the AP.

Wi-Fi scanning uses the core's asynchronous scan API and is deferred during an
association attempt. It returns at most 32 networks. The scanning notice clears
when the scan completes, including an empty result.

While the AP is active, wildcard DNS resolves hostnames to its address. HTTP
connectivity probes, including Windows, Apple and Android probe paths, are
redirected to the portal. Automatic opening depends on the operating system;
VPNs, disabled captive detection or encrypted DNS/probes can prevent it.
HTTPS is not intercepted. If no window opens, use `http://192.168.4.1/` directly.
DNS stops when the AP stops.

![Device configuration](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo2.png)

![Wi-Fi configuration](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo3.png)

![AP configuration](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo4.png)

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)

## Runtime portal standby

The unreleased `suspendPortal()` / `resumePortal()` API turns Wi-Fi and portal
services off and restores the previous AP/reconnect policy without erasing
settings. `portalSuspended()` reports the state. Zigbee or Matter over Thread
can continue; Matter over Wi-Fi rejects suspension. Calls are loop-owned and
can fail while configuration, OTA or scans are busy. Resume starts networking
asynchronously, so check connectivity separately. See [Portal API](Portal-API#suspending-portal-networking).

For commissioning, the ArdUI AP provides configuration and development pairing
codes. Put the phone on the home Wi-Fi before adding Matter in HA; the setup AP
is not the operational Matter network. ESP home-Wi-Fi association is needed for
Matter Wi-Fi, while Matter Thread uses its own mesh credentials and border
router. See [Matter AP commissioning](Matter#commissioning-while-the-portal-uses-ap-mode).
