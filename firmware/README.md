# ESP32 firmware (shared: Critical + Routine)

One firmware codebase for both ESP32 traffic generators. Critical and Routine
differ only by traffic profile; protocol, ACK, timing, and logging logic is
shared. The protocol contract is defined in
[`docs/architecture.md`](../docs/architecture.md#udp-message-and-acknowledgement-protocol-v1)
and the traffic settings in [`docs/testing.md`](../docs/testing.md#fixed-traffic-settings).

## Status

| Piece | State |
|---|---|
| Shared `Q1` formatter + Critical/Routine profiles | implemented, host-tested |
| `A1` ACK parsing, ACK matching, timeout/late handling | not started (next milestone) |
| 200 ms / 50 ms generation schedule, phase labels | not started |
| Sender CSV logging (`run_id,class,seq,phase,send_ms,ack_ms,rtt_ms,ack_status`) | not started |
| Wi-Fi/UDP send and DSCP/TOS marking | not started (hardware) |
| Timed-run start/stop trigger | not started (needs team agreement) |

All host tests below are **software checks**. They do not prove Wi-Fi
operation, real packet marking, timing accuracy, or RTT on a device.

## Layout

```text
firmware/
  shared/
    qos_protocol.h   protocol constants, profiles, Q1 formatter API
    qos_protocol.c   profile values, run-id validation, Q1 formatter
  tests/
    test_q1_packet.c host tests for the formatter (C99, no dependencies)
    Makefile         builds in tests/build/ (not part of results/)
```

The protocol code has no Arduino/ESP-IDF dependencies so it can be compiled
and tested on the development host. Hardware code (Wi-Fi, UDP, TOS) will live
next to it later and call the same shared functions.

## Host tests

With `make` (Linux/macOS/WSL; on MinGW use `make CC=gcc`):

```sh
cd firmware/tests
make
```

Without `make`, compile directly — the same flags work with MinGW GCC,
`clang`, or `cc` (PowerShell shown):

```powershell
cd firmware/tests
New-Item -ItemType Directory -Force build | Out-Null
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -I../shared `
  -o build/test_q1_packet.exe test_q1_packet.c ../shared/qos_protocol.c
.\build\test_q1_packet.exe
```

Run from the `firmware/tests` directory so the relative `../shared` include
and source paths resolve. Test output goes to `firmware/tests/build/`, never
`results/`.

Expected output:

```text
PASS: <n> checks, 0 failure(s)
```

The tests check the Critical payload is exactly 128 bytes, the field contents
(`Q1|<run_id>|critical|<sequence>`), ASCII `0x20` padding only after the final
field, absence of NUL/newline/CR bytes, no write past the payload, rejection of
invalid run IDs, sequence `0`, short buffers and inconsistent profiles, and
that the Routine profile reuses the same formatter at 256 bytes.

## Pending: board and toolchain

The repository does not yet state which ESP32 model (classic ESP32, C3, S3,
...) or framework (Arduino-ESP32 vs ESP-IDF) the team has. No board-specific
project file is committed until that is confirmed, so the choice is not baked
in by accident.

Once the board is known, the natural next step is a PlatformIO or
Arduino-ESP32 application in `firmware/src/` that:

1. includes `shared/qos_protocol.h` and calls `qos_format_q1()`;
2. opens one UDP socket to `<NUC address>:5001` and reuses it for ACKs;
3. sets the socket's IP TOS (`0xA0` for Critical, `0x00` for Routine) — this
   must be verified in captured packets, not assumed;
4. sends on a monotonic 200 ms / 50 ms schedule and never retransmits.

Pending target compile check (record the output once a toolchain exists):

```sh
pio run -d firmware        # or: arduino-cli compile --fqbn <fqbn> firmware
```

## Not yet defined here

- The timed-run start/stop trigger. `docs/testing.md` requires the team to
  agree on one mechanism before any component implements it.
- Real Wi-Fi credentials. Keep them out of Git; use a local, untracked
  configuration when the app is added.
