# ESP32 firmware (shared: Critical + Routine)

One firmware codebase for both ESP32 senders. Critical and Routine use the
same protocol, lifecycle, ACK and logging code and differ only by traffic
profile (`QOS_PROFILE_CRITICAL` / `QOS_PROFILE_ROUTINE`).

Specifications: [`docs/architecture.md`](../docs/architecture.md#udp-message-and-acknowledgement-protocol-v1)
and [`docs/testing.md`](../docs/testing.md#fixed-traffic-settings).

## What is implemented

| Piece | State |
|---|---|
| `Q1` formatter, profile table, run-id rules | done, host-tested |
| `A1` parsing (strict, unpadded) | done, host-tested |
| Sequence tracking, 200 ms / 50 ms scheduling | done, host-tested |
| Warm-up → measure → drain → done lifecycle | done, host-tested |
| ACK matching, RTT, 2000 ms timeout, late ACKs | done, host-tested |
| Sender CSV rows | done, host-tested |
| UDP transport (Winsock / POSIX / lwIP) | done; host-verified on localhost |
| ESP32 app (Wi-Fi join, Serial CSV) | written, **never compiled or run on a device** |
| DSCP/TOS marking actually on the wire | requested in code, **unverified** |
| Team-agreed run trigger | not implemented (see below) |

Host tests prove protocol and lifecycle logic. They do **not** prove Wi-Fi
operation, gateway QoS, DSCP treatment, on-device timing or real RTT.

## Layout

```text
firmware/
  shared/                 PlatformIO library + host-compiled core
    src/qos_protocol.*    Q1 format, A1 parse, profiles, run-id rules
    src/qos_sender.*      lifecycle, sequences, ACK/timeout/RTT, CSV rows
    src/qos_udp.*         IPv4 UDP transport (Winsock/POSIX/lwIP)
  host/main.c             CLI sender over real UDP sockets (development tool)
  esp32/critical/         PlatformIO app for the Critical device
  tests/                  host tests + mock receiver (test-only, never results/)
```

The core has no Arduino/ESP-IDF dependencies, which is why the same files run
in the host tests, the host CLI and the ESP32 app.

## 1. Run the host tests

```sh
cd firmware/tests
make          # on MinGW: make CC=gcc
```

Expected:

```text
PASS: 448 checks, 0 failure(s)      # Q1 payload format
PASS: 155 checks, 0 failure(s)      # A1, lifecycle, ACK/RTT, CSV
```

Without `make` (PowerShell, MinGW GCC):

```powershell
cd firmware/tests
New-Item -ItemType Directory -Force build | Out-Null
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -I../shared/src `
  -o build/test_q1_packet.exe test_q1_packet.c ../shared/src/qos_protocol.c ../shared/src/qos_sender.c
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror -I../shared/src `
  -o build/test_sender.exe test_sender.c ../shared/src/qos_protocol.c ../shared/src/qos_sender.c
.\build\test_q1_packet.exe
.\build\test_sender.exe
```

## 2. Send real packets from the host

This exercises the real sender core against a real UDP socket. It is useful
for receiver interoperability and for seeing the sender CSV, but a localhost
run is **not** evidence about Wi-Fi or gateway QoS.

```sh
cd firmware/host
make                    # on MinGW: make CC=gcc

./build/qos_sender_host --run-id b01_fifo --dest 127.0.0.1 --port 5001 \
    --warmup-ms 1000 --measure-ms 3000 --drain-ms 1000 --timeout-ms 500 \
    --csv /tmp/critical.csv
```

Defaults match the specification (`192.168.60.2:5001`, 10 s warm-up, 60 s
measure, 2 s drain, 2 s ACK timeout). Short durations are for local testing
only. Progress and counters go to stderr; the CSV goes to `--csv` or stdout.

To test without the NUC, run the mock receiver in another terminal:

```sh
python3 firmware/tests/mock_receiver.py --port 5001 --drop-every 7
```

It validates length/fields/padding, answers with unpadded `A1`, and can
`--drop-every N` or `--delay-every N --delay-ms 2500` to produce timeouts and
late ACKs. Its output is test data, never formal results.

### Sender CSV

```text
run_id,class,seq,phase,send_ms,ack_ms,rtt_ms,ack_status
b01_fifo,critical,1,warmup,2615642968,2615642984,16,on_time
b01_fifo,critical,4,measure,2615661953,,,none
b01_fifo,critical,5,measure,2615662156,2615662750,594,late
```

- `send_ms` / `ack_ms` / `rtt_ms` use the sender's own monotonic clock.
- `ack_status`: `on_time` (within the ACK timeout), `late` (timed out, ACK
  arrived later; `timed_out` still counts it) or `none` (never acknowledged).
- There are no application-level retransmissions: one sequence, one datagram.

## 3. Build for the ESP32

Board model and framework are **not** confirmed in this repository, so
`esp32/critical/platformio.ini` uses `board = esp32dev` as a placeholder.
Change it to the real board (for example `esp32-c3-devkitm-1`) before
flashing. No compile for the target has been performed here.

```sh
cd firmware/esp32/critical
cp include/config.example.h include/config.h   # then edit credentials
pio run -t upload
pio device monitor -b 115200
```

`include/config.h` is gitignored. The app joins the Pi AP, opens a UDP socket
to `NUC_IP:NUC_PORT`, requests TOS `0xA0`, runs one warm-up/measure/drain run
automatically after boot and prints the CSV rows and counters over Serial.

**Run trigger:** `docs/testing.md` requires the team to agree on one trigger
mechanism before timed runs. This app starts one run at boot and never
restarts, deliberately avoiding a private control protocol. Replace that with
the agreed trigger when it exists.

## Verified locally (and what it does not prove)

Verified on the development host with GCC 16.2 (`-Wall -Wextra -Wpedantic
-Werror`) and Python 3.13:

- 603 host checks pass (payload format, A1 parsing, scheduling, phases,
  ACK/timeout/late, duplicate handling, CSV rows, millisecond wraparound).
- End-to-end localhost runs against the mock receiver: 7 messages, 7 ACKs,
  measured RTTs; and a run with a dropped ACK and a delayed ACK produced
  `none` and `late` rows with `timed_out=3`.

Still unverified, and requiring hardware:

- ESP32 compilation for the real board, Wi-Fi association and UDP path.
- Actual DSCP/TOS in captured packets (`0xA0` for Critical).
- Real generation timing, RTT, ACK loss and deadline behaviour.
- Pi routing, Ethernet bottleneck, QoS scenarios (other components).
