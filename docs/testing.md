# Testing and experiment specification

## Purpose and boundary

Test whether **fixed gateway QoS** improves Critical-message performance when traffic from Wi-Fi clients competes for a controlled **5 Mbit/s Raspberry Pi 5 Ethernet egress bottleneck**.

Packet path: ESP32 Critical + ESP32 Routine + MacBook Bulk (Wi-Fi) → Pi 5 AP/router (shape on Ethernet egress) → Lumi's housemate's Kubuntu NUC (wired receiver).

Only the **Pi's outbound Ethernet** is shaped. ACKs travel back from the NUC over the reverse path. This is not a Wi-Fi airtime/WMM test, adaptive QoS implementation, or proof of emergency-system reliability. The [architecture and UDP protocol](architecture.md) define device addressing and message/ACK formats.

**Status:** These are the **selected implementation and experiment targets**, not measured or hardware-verified results. Complete the short pilot checks below, record any necessary changes, and **freeze settings before the 15 formal runs**. Never silently substitute simulation results for measurements.

## Software-only preflight (before hardware access)

Develop and check the components locally **before** the physical-network pilots:

- **NUC receiver:** use a UDP client over `127.0.0.1` to test the real `Q1`/`A1` format, exact 128/256-byte payloads, invalid messages, duplicate sequences, ACK matching, and the receiver CSV schema.
- **ESP32 firmware:** test shared packet generation/parsing, sequence numbers, ACK timeout/late-ACK handling, and Critical/Routine profile settings on a host where possible. Compile for the selected ESP32 board when the model/toolchain is available; record the distinction between host tests and target compilation.
- **Pi gateway:** review scripts for the specified 5 Mbit/s FIFO and HTB configurations, DSCP filters, inspection and rollback commands. Optional disposable Linux namespace/VM tests can check syntax and queue behaviour without touching the real Pi.
- **MacBook Bulk:** test `iperf3` UDP client/server locally and check that run-specific JSON is generated. Later confirm the installed Mac version and actual packet DSCP markings.
- **Cross-component:** use clearly marked synthetic fixtures to exercise CSV joins by `(run_id, class, seq)` and basic calculation logic without reporting those numbers as experiment results.

Store synthetic/local test output in `tests/fixtures/`, other clearly named test directories, or temporary directories; **never in `results/<run_id>/` formal data**. Software checks prove only the behaviour they directly test; they cannot establish Pi Wi-Fi AP operation, real routing, congestion at Ethernet egress, radio behaviour, effective DSCP treatment, or any QoS improvement. All of those require the hardware pilots below.

## Fixed traffic settings

| Traffic | Source | Offered traffic | UDP payload | DSCP | Destination |
|---|---|---|---|---|---|
| Critical | ESP32 | 5 messages/s (one every 200 ms) | 128 bytes | `CS5` (DSCP 40, IPv4 TOS `0xA0`) | NUC `192.168.60.2:5001` |
| Routine | ESP32 | 20 messages/s (one every 50 ms) | 256 bytes | `CS0` (DSCP 0, TOS `0x00`) | NUC `192.168.60.2:5001` |
| Bulk | MacBook on Pi Wi-Fi | 8 Mbit/s UDP when enabled | 1200 bytes | `CS1` (DSCP 8, TOS `0x20`) | NUC `192.168.60.2:5201` |

- Critical and Routine use [the same text UDP protocol](architecture.md#udp-message-and-acknowledgement-protocol-v1) and do **not** retransmit. Their payload rate, excluding UDP/IP/Wi-Fi overhead, is **5.12 kbit/s** and **40.96 kbit/s**, respectively.
- The MacBook runs `iperf3` as a **UDP client**; the NUC runs the server. Keep the UDP payload size, offered bitrate, and packet markings constant between congested scenarios.
- The ESP32s transmit the same marked traffic in **all three scenarios**. Marking is not an experimental variable. Verify each DSCP value on packets actually reaching the Pi/NUC; macOS may not preserve the requested value without verification.
- Use the same Pi AP, Wi-Fi channel, device positions, power settings, firmware, and receiver for all comparable runs. Avoid unrelated downloads/updates and ensure the MacBook uses the experiment Wi-Fi network.
- `CS5`, `CS0`, and `CS1` are **packet header markings**; the separate Linux traffic-control classifier must match those packets to the intended classes. Do not assume Wi-Fi prioritisation has been demonstrated.

## Gateway shaping and queues

Apply both treatments to the **same Pi Ethernet egress interface** (expected `eth0`; verify actual interface name). Keep the aggregate egress limit at **5 Mbit/s**, including in Baseline.

| Setting | Shared FIFO | Classified fixed QoS |
|---|---|---|
| Shaper/scheduler | Root `tbf` at 5 Mbit/s, using its FIFO queue | Root/parent `htb` at 5 Mbit/s, with three child classes |
| Nominal total waiting budget | 100 KiB | 100 KiB divided across child queues |
| Critical class | Not separately classified | `htb rate 500kbit ceil 5mbit prio 0`; 16 KiB `bfifo` |
| Routine class | Not separately classified | `htb rate 1mbit ceil 5mbit prio 1`; 24 KiB `bfifo` |
| Bulk class | Not separately classified | `htb rate 3500kbit ceil 5mbit prio 2`; 60 KiB `bfifo` |
| Classification | One shared queue, irrespective of DSCP | Classify `CS5` → Critical, `CS0` → Routine, `CS1` → Bulk; an explicit default class handles unmatched packets |

**HTB class mapping:** parent class `1:1` (5 Mbit/s); `1:10` = Critical, `1:20` = Routine, `1:30` = Bulk. Use `1:20` as the explicit default for any unmatched traffic; unmatched packets must not silently bypass the configured egress shaper. On the expected IPv4 path, `tc flower` can match the DSCP portion of the TOS byte using `ip_tos 0xA0/0xFC` (Critical), `0x00/0xFC` (Routine), and `0x20/0xFC` (Bulk), so ECN bits do not affect the match. Verify supported syntax and counters on the Pi. The expected MacBook Bulk packets must match `1:30`, not fall back to Routine.

In HTB, `rate` is a configured class allocation and `ceil` permits borrowing unused bandwidth; it is **not** a promise that each class continuously consumes its allocated rate. Lower `prio` values take precedence in HTB's borrowing decisions. With low sensor traffic, Bulk may use more than 3.5 Mbit/s when spare capacity exists.

For an initial implementation, use `tbf` with a documented `burst` (starting target: 32 KiB) and `limit 102400` bytes for shared FIFO. For HTB, attach byte-limited `bfifo` leaves of **16384 / 24576 / 61440 bytes** respectively; document all parent/child burst, quantum, and queue settings actually used. These choices are **pilot-check targets**: `tbf` and `htb` differ internally, so matching nominal queue bytes does not make their behaviour identical. The experiment compares complete **FIFO and classified QoS treatments**, not an isolated scheduling algorithm.

Before formal runs, verify the effective 5 Mbit/s cap, DSCP classifier hits for all three streams, queue counters, and actual load. If the bottleneck is instead on Wi-Fi or a receiver/process, the run is not a valid test of the intended gateway output.

## Scenarios

| Scenario ID | Bulk | Treatment on Pi Ethernet | Purpose |
|---|---|---|---|
| `baseline` | Off | Shared FIFO; 5 Mbit/s cap | Establish low-load sensor performance |
| `fifo` | 8 Mbit/s UDP | Shared FIFO; 5 Mbit/s cap | Measure unclassified gateway congestion |
| `qos` | Same 8 Mbit/s UDP | Classified HTB QoS; same 5 Mbit/s cap | Measure the classified-treatment trade-off |

The QoS scenario changes **both scheduling and queue admission** via its separate class queues. Do not attribute any improvement exclusively to scheduler priority.

## Run coordination contract (before timed runs)

The UDP `Q1`/`A1` protocol covers **sensor data and acknowledgements only**. **Do not independently invent run-control packets or APIs** for the ESP32s, NUC, and MacBook. Before building timed-run automation, the team must agree on a single simple command/trigger mechanism (for example, serial/CLI control or a documented shared trigger) and document how it reaches each component.

Every implementation must support the same **logical run lifecycle**:

1. **Prepare:** configure the same `run_id` and scenario on both ESP32 senders and the receiver; identify the Bulk output file and Pi treatment. The NUC receiver/logger must be ready before transmissions begin.
2. **Start:** use the agreed trigger/procedure to begin a run. Each participant records its **actual local start time** and phase boundaries; do not assume clocks are synchronised or that commands arrive simultaneously.
3. **Warm-up → measure:** run 10 seconds of warm-up followed by 60 seconds of measured traffic. Each ESP32 labels sent sequences `warmup` or `measure` using its own monotonic clock; the MacBook's `iperf3` warm-up and measured interval should be aligned by the agreed procedure.
4. **Stop and drain:** stop new sends after the 60-second measurement interval. Keep the NUC receiver and ESP32 ACK listeners available for 2 additional seconds; record late completions, then save logs. An early stop/abort must be recorded as such.

**Unresolved integration decision:** the trigger's **transport, initiator, and alignment/verification method**. Resolve them together, document the chosen procedure here, and test alignment in a pilot **before any formal timed run**. Initial one-packet and localhost tests do not need this control mechanism. If alignment cannot be demonstrated, do not treat runs as comparable.

## Run procedure

Use **5 blocks × 3 scenarios = 15 runs**. Preselect and record the scenario order within each block to limit order effects; each scenario occurs once per block.

| Block | Run 1 | Run 2 | Run 3 |
|---|---|---|---|
| `b01` | `baseline` | `fifo` | `qos` |
| `b02` | `qos` | `baseline` | `fifo` |
| `b03` | `fifo` | `qos` | `baseline` |
| `b04` | `baseline` | `qos` | `fifo` |
| `b05` | `fifo` | `baseline` | `qos` |

Name each run `<block>_<scenario>`, e.g. `b01_fifo`; configure the **identical run ID on both ESP32s and NUC**. All run IDs satisfy the UDP protocol's character/length rules.

For **each run**:

1. Apply the scenario's `tc` configuration; confirm the interface, rates, queues, filters, and DSCP classification. Start NUC logging and `iperf3` server **before** senders.
2. Record run ID, scenario, block, devices/software versions, Git commit, relevant Wi-Fi settings, IPs, `tc` configuration, and sender traffic settings. Save the initial `tc` counters.
3. Start both ESP32 senders and (for congested scenarios) the MacBook `iperf3` client. Use the agreed [run coordination contract](#run-coordination-contract-before-timed-runs), and log **per-device warm-up/measurement boundaries**; do not assume manually started clocks are synchronised.
4. **Warm up for 10 seconds**, then collect a **60-second measurement interval** at the same traffic settings. Each ESP32 marks which emitted sequences belong to the measurement interval. Expected measured generation if no application sends are missed: **300 Critical** and **1200 Routine** packets per run.
5. Stop generating new packets at the end of the measurement interval. Keep the receiver and ACK listener running for a **2-second drain period** to observe completions and late ACKs; save stop/end timestamps. Record the final `tc` counters and `iperf3` output.
6. Preserve raw logs and settings in `results/<run_id>/`. Do not start the next run until the previous run's logs are safely saved and the gateway queue is cleared/reconfigured.

Do not start formal runs until the chosen trigger/procedure has demonstrated reproducible 10 s/60 s phase alignment in a pilot. Separate monotonic clocks are fine for per-ESP32 RTT, but do not use them as though they were synchronised.

### Bulk traffic commands (starting point)

On the Kubuntu NUC, start the server before each test session:

```sh
iperf3 -s -p 5201
```

On the **MacBook**, for a congested scenario (replace the example output path with the run-specific filename):

```sh
iperf3 -4 -c 192.168.60.2 -p 5201 -u -b 8M -l 1200 \
  -t 60 -O 10 -S 0x20 -J > bulk-b01_fifo.json
```

With `-O 10` and `-t 60`, `iperf3` performs a 10-second pre-test **in addition to** the 60-second measured interval. `-S 0x20` requests CS1/TOS marking; **verify it in captured packets** on the actual macOS/Pi path. Do not run the Bulk client in Baseline.

On the Pi, capture queue/class/filter evidence around the measurement interval:

```sh
sudo tc -s qdisc show dev eth0
sudo tc -s class show dev eth0
sudo tc -s filter show dev eth0
```

These are inspection commands, **not** a complete configured `tc` implementation. Save a per-scenario QoS setup script as code when implementing the gateway.

## Measurements and definitions

Calculate results from the **60-second measurement interval** only; exclude warm-up sends. Match sender and receiver records by `(run_id, traffic_class, sequence)`. Receiver observation may extend through the **2-second drain**; keep late arrivals visible in logs.

| Metric | Calculation / source |
|---|---|
| **Receiver delivery ratio** | Unique valid messages received on the NUC within the observation window ÷ messages actually sent in the measured interval, per ESP32 stream. Use sender's **actual sent count** for the denominator. |
| **RTT (ms)** | Each ESP32: matching ACK receive time minus its original send time, on the **same monotonic clock**. |
| **p95 RTT (ms)** | 95th percentile of matched ACK RTTs received within the **2000 ms ACK timeout**, per class/run. Also report the number of RTT samples. |
| **Deadline success (%)** | Messages with a matching ACK received **within 100 ms** of sending ÷ all messages sent in the measured interval. This is an **ACK-based round-trip deadline**, not a measured forward-only deadline. |
| **ACK timeout rate (%)** | Measured sends without matching ACK within **2000 ms** ÷ all measured sends. Log late ACKs separately if they arrive after timeout. |
| **Bulk goodput** | NUC-side received `iperf3` UDP bits ÷ measured time; preserve `iperf3` receiver loss/jitter statistics. |
| **Gateway queue evidence** | Before/after `tc` qdisc/class/filter counters: sent bytes/packets, drops, backlog, overlimits where available, and per-class classification matches. |

The **100 ms deadline** and **2000 ms timeout** are starting thresholds to assess during pilots and freeze **before** formal collection. For receiver delivery, do **not** infer loss from missing ACKs: the NUC log is authoritative for successful forward delivery. Report measured RTT distributions alongside ACK failures; successful-only p95 can hide losses/timeouts.

Keep run-level results separate; compare medians/ranges across five repeats rather than pooling all messages as though 15 runs were independent packet samples. Report whether QoS reduces Critical RTT/deadline misses and what it costs Bulk in goodput or drops.

## Log files and fields

Use one directory per run. Plain CSV/JSON/text is sufficient—no database or monitoring platform required.

```text
results/
  b01_fifo/
    metadata.json
    critical.csv
    routine.csv
    receiver.csv
    bulk.json
    tc-before.txt
    tc-after.txt
```

**Required sender CSV columns:** `run_id,class,seq,phase,send_ms,ack_ms,rtt_ms,ack_status`, where `phase` is `warmup` or `measure`; `ack_status` is `on_time`, `late`, or `none` (here `on_time` means within the **2000 ms timeout**, not the 100 ms deadline). A late ACK must remain recognisable as having **timed out**.

**Required NUC receiver CSV columns:** `run_id,class,seq,recv_ms,source_ip,source_port,bytes,duplicate,valid`. Keep malformed packets identifiable, even where identifiers cannot be parsed; receiver times are **NUC-local** and must never be subtracted from ESP32 timestamps.

**Metadata:** scenario, block, timestamps and phase alignment method, actual traffic parameters, IP addresses, interface names, OS/tool versions, software commit, queue/shaper settings, any deviations, and validity notes. For Baseline `bulk.json` may be omitted or marked not applicable. Raw results must never be fabricated.

## Pilot checklist and invalid runs

**Pilot checks (not formal results):**

- End-to-end UDP + ACK works for **both ESP32s** through the Pi's Ethernet egress, with the intended 128/256-byte payload lengths.
- NUC receives MacBook UDP, and its `iperf3` server reports an offered-load test. Confirm packets are shaped at the **Pi egress**, not blocked by low Wi-Fi capacity or a host firewall.
- Packet captures verify `CS5`, `CS0`, `CS1` and each HTB class counter increases under expected traffic.
- Congested FIFO produces evidence of queueing or drops with the 8 Mbit/s offered load and 5 Mbit/s shaper; low-load Baseline is not unintentionally congested.
- Both senders and the NUC agree on run ID and measurement boundaries; CSV joins and denominator counts work.
- Evaluate whether 100 ms and 2000 ms thresholds give useful discrimination without frequent Baseline failures; if adjusted, record and freeze the new values **before** the first formal run.

**Mark a run invalid** if any required stream never starts/stops as planned; network routing/interface/shaping is wrong; classification does not match the specification; the intended gateway bottleneck is absent; measurement boundaries are not recoverable; or required logs are missing/corrupt. Record **why** and keep its raw data. Rerun the same block/scenario with a distinct replacement identifier (e.g. `b01_fifo_retry1`, within 24 characters). Do not silently drop inconvenient results.

## References for implementation

- [iperf3 command options](https://software.es.net/iperf/invoking.html) — UDP, `-b`, `-l`, `-S`, `-O`, `-J`.
- [Linux `tc-htb` manual](https://man7.org/linux/man-pages/man8/tc-htb.8.html) — class rates, ceilings, priorities and borrowing.
- [Linux `tc-tbf` manual](https://man7.org/linux/man-pages/man8/tc-tbf.8.html) and [`tc-bfifo` manual](https://manpages.debian.org/trixie/iproute2/tc-bfifo.8.en.html) — shared FIFO shaping and byte-limited leaf queues.
- [Linux `tc-flower` manual](https://man7.org/linux/man-pages/man8/tc-flower.8.html) — matching DSCP/TOS values using an explicit mask.
