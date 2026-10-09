# Allocation-refusal regression — 2026-10-08

Hardware: ESP32-D0WDQ6 revision 1.0, Arduino ESP32 core 3.3.12,
FQBN `esp32:esp32:esp32doit-devkit-v1`.

Built with `compiler.c.elf.extra_flags=-Wl,--wrap=malloc` and uploaded over USB.
The test refuses JSON member and traversal-array allocations on the Arduino task,
first during startup and again after five seconds of normal portal operation.

Observed UART output:

```text
PASS: Member and array OOM; JSON recovered
PASS: runtime OOM; HTTP/UART continue
ARDPORTAL_UART_ECHO_CHECK
```

The final line was sent from the host and echoed by the device. Seventeen periodic
loop reports continued without another boot banner or an abort/backtrace. After
runtime allocation refusal, the following HTTP checks succeeded:

| Request | Result |
| --- | --- |
| `/` | HTTP 200, 6247 bytes |
| `/api/status` | HTTP 200, 401 bytes |
| `:8080/` | HTTP 200, `PASS loopTicks=99288` |
| `:8080/` again | HTTP 200, `PASS loopTicks=100681` |

Host ASan/UBSan checks additionally covered refused JSON member/key/array
allocations, recovery, preservation of a previous traversal buffer on resize
failure, direct/transitive invalidation, unrelated telemetry during an active HA
traversal, and one-second Discovery retry backoff. Async registration and a build
with HA/dependencies disabled passed separately.

These checks exercise explicit allocation refusal, not every possible SDK-internal
failure or radio/flash timing condition.
