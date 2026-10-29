# ArdFS versus LittleFS firmware size — 2026-10-29

Current production sources, including optimized ArdFS V3 physical fragmentation.
Application `.bin` sizes for the same `DynamicPages` example:

| Target | ArdFS | LittleFS | LittleFS increase |
| --- | ---: | ---: | ---: |
| ESP8266 Generic, core 3.1.2 | 592176 B | 607568 B | 15392 B (15.03 KiB) |
| ESP32 Dev Module, core 3.3.12 | 1282112 B | 1310096 B | 27984 B (27.33 KiB) |

These are firmware image sizes, not filesystem-partition usage. Results depend
on the sketch, board and core. They include the shared ArdStorage transaction
engine, application and relevant core filesystem implementation; they are not
standalone filesystem-library sizes.

Native clean builds are the `opt7` measurements from the preceding optimization
report; every production source hash was rechecked unchanged. LittleFS builds
used separate snapshots with the same source files, core configuration and
board defaults, changing only a sketch-local `ARDPORTAL_USE_LITTLEFS 1` before
all includes. Both new LittleFS clean builds succeeded. No special linker or
optimization flags were added. Target symbol tables and all dependency files
confirm that LittleFS excludes the native ArdFS implementation.

ESP32 compiler program-storage figures are 1281858 B with ArdFS and 1309846 B
with LittleFS; binary padding explains the slight difference from image sizes.
The default ESP32 application partition is 1310720 B, leaving 28862 B / 874 B
of compiler-reported program capacity respectively. Static RAM is 43740 B /
44028 B on ESP8266 and 57724 B / 57636 B on ESP32 (ArdFS / LittleFS).
These figures do not measure runtime heap peaks or filesystem performance.

Artifacts and compiler logs: `/tmp/ardfs-fragments-20261029`, snapshots `opt7`
and `backend-littlefs`; logs `<board>-opt7.log` and
`<board>-backend-littlefs.log`. Binaries, ELFs and sketch dependency files are
preserved in the corresponding `-out` directories. Build intermediates were
removed to recover disk space. The adjacent JSON contains source and image
hashes, sizes and backend-exclusion verification.
