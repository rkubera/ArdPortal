# Portal screenshots

[Documentation home](https://github.com/rkubera/ArdPortal/wiki/Home) · [All topics](https://github.com/rkubera/ArdPortal/wiki/Home#start-here) · [Source](https://github.com/rkubera/ArdPortal)

## On this page

- [Home](#home)
- [Device](#device)
- [Wi-Fi configuration](#wi-fi-configuration)
- [AP configuration](#ap-configuration)
- [MQTT configuration](#mqtt-configuration)
- [Firmware update](#firmware-update)
- [MQTT Console](#mqtt-console)
- [Dynamic sensor settings](#dynamic-sensor-settings)
- [Zigbee network](#zigbee-network)
- [Matter over Wi-Fi](#matter-over-wi-fi)
- [Matter over Thread](#matter-over-thread)

These views use the current ArdPortal frontend with illustrative configuration,
network states and readings. Available pages depend on compile-time features;
Zigbee and Matter are shown as separate firmware profiles. Addresses, broker
settings and values are examples. The Matter QR is a public development example,
not a code for a deployed device.

## Home

Home values and device information.

![Home](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo1.png)

## Device

Device identity, restart and factory-reset actions.

![Device](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo2.png)

## Wi-Fi configuration

Network scan with signal/security indicators and 2.4/5 GHz labels, plus saved station credentials.

![Wi-Fi configuration](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo3.png)

## AP configuration

Configuration access point, band selection on supported hardware and password.

![AP configuration](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo4.png)

## MQTT configuration

Broker host, transport selection, port and credentials; TCP is selected in this view.

![MQTT configuration](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo5.png)

## Firmware update

File upload or browser download from a firmware URL.

![Firmware update](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo6.png)

## MQTT Console

Received and queued MQTT traffic, plus manual publishing.

![MQTT Console](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo7.png)

## Dynamic sensor settings

A calibration slider and named MQTT update options on an application-defined page.

![Dynamic sensor settings](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/Demo8.png)

## Zigbee network

Network identity, native endpoint count, rejoin/reset actions and layout-specific
Zigbee2MQTT/ZHA downloads. Role is selected at compilation.
[Zigbee guide](Zigbee).

![Zigbee network and gateway exports](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/zigbee-network.png)

## Matter over Wi-Fi

Transport, credential profile, endpoint counts, development pairing QR/manual code
and pairing/reset actions. Production credentials use the provisioned device
label instead of displaying a development code. [Matter guide](Matter).

![Matter over Wi-Fi with development pairing information](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/matter-wifi.png)

## Matter over Thread

Matter uses Thread while Wi-Fi provides the portal. The status bar distinguishes
Matter connectivity from Thread attachment. [Transport profiles](Matter#esp-idf--esp-matter-build).

![Matter over Thread and separate network status](https://raw.githubusercontent.com/wiki/rkubera/ArdPortal/assets/matter-thread.png)

---

[Back to documentation home](https://github.com/rkubera/ArdPortal/wiki/Home)
