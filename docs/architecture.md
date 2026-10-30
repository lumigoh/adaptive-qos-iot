# Hardware architecture

## Goal and scope

Build a small Wi-Fi IoT network and compare how a Raspberry Pi 5 gateway handles Critical, Routine, and Bulk traffic when its **Ethernet output** becomes congested. The primary experiment tests **fixed gateway QoS**, not adaptive control or Wi-Fi airtime prioritisation.

```text
ESP32 (Critical) ─┐
ESP32 (Routine) ──┼── Wi-Fi ──> Raspberry Pi 5 ── Ethernet ──> Kubuntu NUC
MacBook (Bulk) ───┘             AP / QoS gateway                Receiver / logger
                               (shaping on eth0 egress)
```

All three senders connect to the Pi's Wi-Fi access point (AP). The Pi **routes**, rather than locally receives, the test traffic. The NUC receives the traffic, sends acknowledgements to ESP32 devices, and runs the `iperf3` server.

**Why the separate receiver matters:** packets sent directly to a service on the Pi would not traverse its Ethernet egress queue, so they could not demonstrate the intended gateway bottleneck.

**Lumi's housemate's Kubuntu NUC** is the planned wired receiver, subject to availability. The **MacBook** generates Bulk traffic over Wi-Fi and should not also act as the wired receiver during the same experiment. A physical Ethernet cable is required between the Pi and NUC.

## Components to implement

| Component | Device | Responsibility |
|---|---|---|
| Critical traffic generator | ESP32 | Generate alert-like UDP messages, apply Critical marking, track sequence numbers and acknowledgements |
| Routine traffic generator | ESP32 | Generate periodic telemetry UDP messages, apply Routine marking, track sequence numbers and acknowledgements |
| Bulk traffic generator | MacBook (Wi-Fi) | Generate configurable competing UDP traffic using `iperf3` |
| AP, router, and QoS gateway | Raspberry Pi 5 | Provide Wi-Fi access, route packets to Ethernet, create the controlled output bottleneck, select FIFO or classified QoS |
| Receiver, ACKs, and logging | Kubuntu NUC (Lumi's housemate) | Receive and identify ESP32 packets, send UDP acknowledgements, run the `iperf3` server, collect results |

The two ESP32 roles may share one configurable firmware codebase; the two devices run different traffic profiles.

## Network and interfaces

**Proposed IPv4 plan** — based on the current project design, to confirm during physical setup:

| Network/interface | Proposed address | Purpose |
|---|---|---|
| Pi Wi-Fi AP (typically `wlan0`) | `192.168.50.1/24` | Gateway for ESP32 devices and MacBook |
| Wi-Fi clients | `192.168.50.0/24` (DHCP) | Send traffic to the wired receiver |
| Pi Ethernet (typically `eth0`) | `192.168.60.1/24` | Routed output and QoS/shaping point |
| Kubuntu NUC Ethernet | `192.168.60.2/24` | Receiver and test server |

- Confirm actual interface names and the Pi operating system before writing setup commands.
- Enable IPv4 forwarding on the Pi. Do not rely on NAT between these two test subnets.
- Ensure the NUC has a return route to `192.168.50.0/24` via `192.168.60.1` (and that host firewall rules allow the test traffic).
- Wi-Fi clients send to the **NUC address** `192.168.60.2`, not to the Pi. The Pi forwards packets to the NUC.
- Prefer explicit IPv4 addresses for the experiment to keep the tested packet path clear.

No address assignment, AP operation, route, or interface name has yet been verified on hardware.

## Application traffic and interfaces

The planned application path is:

1. An ESP32 sends a UDP message to the NUC through the Pi.
2. The NUC records the message identity and receipt, then replies with a UDP acknowledgement to the sender.
3. The ESP32 matches the acknowledgement to the original sequence number and measures **round-trip time (RTT)** with its own monotonic clock.
4. Independently, the MacBook sends Bulk UDP traffic through the same Pi Ethernet output to the NUC's `iperf3` server.

**Proposed Differentiated Services Code Point (DSCP) markings:**

| Traffic | Marking | Sender |
|---|---|---|
| Critical | `CS5` | ESP32 Critical |
| Routine | `CS0` | ESP32 Routine |
| Bulk | `CS1` | MacBook `iperf3`; explicitly configure the marking |

Keep these markings **unchanged across FIFO and classified-QoS scenarios** so the experiment changes gateway scheduling, not the senders' Wi-Fi treatment. The marks and their mapping to Linux `tc` classes must be verified with packet captures/counters.

### UDP message and acknowledgement protocol (v1)

This is the **shared implementation contract** for both ESP32 senders and the NUC receiver. It is a simple UDP/IPv4 ASCII protocol; the MacBook's `iperf3` traffic remains independent.

| Setting | Value |
|---|---|
| NUC message destination | Proposed `192.168.60.2:5001` |
| NUC Bulk `iperf3` server | UDP port `5201` |
| ESP32 source port | Any local UDP port; reuse the same socket to receive ACKs |
| Encoding | ASCII, fields separated by `|`; no trailing newline |
| Critical data payload | Exactly **128 bytes**, including padding |
| Routine data payload | Exactly **256 bytes**, including padding |

**Data message (ESP32 → NUC):**

```text
Q1|<run_id>|<traffic_class>|<sequence>
Q1|r001|critical|42
```

- `Q1` means a version-1 data message.
- `run_id` is shared by both senders and the receiver for one experiment run: 1–24 ASCII letters, digits, `_`, or `-` (for example `b01_fifo`).
- `traffic_class` is exactly `critical` or `routine`. Each sender uses its configured class.
- `sequence` is a positive decimal integer, starting at `1` and increasing by one for **each generated message per sender per run**. No sequence number is reused within that stream.
- Append ASCII spaces (`0x20`) **after** the final field until the UDP payload is exactly 128 bytes for Critical or 256 bytes for Routine. Do not add a newline, NUL terminator, or extra fields. Only the sender pads; the receiver removes trailing padding spaces before parsing.
- The format is intentionally text-based for easy debugging. It does not transmit a sender timestamp; RTT uses the sender's local clock.

**Acknowledgement (NUC → originating ESP32):**

```text
A1|<run_id>|<traffic_class>|<sequence>
A1|r001|critical|42
```

- `A1` identifies a version-1 ACK. Repeat the exact three identifiers from the accepted data packet.
- ACK payloads are **unpadded ASCII**, with no newline.
- Send the ACK from the NUC's UDP receiver socket (port `5001`) to the **source IP and UDP source port** of the received datagram.
- Each ESP32 keeps its sending socket open for ACKs and accepts only ACKs matching its current run, class, and an issued sequence number. Validate the expected NUC endpoint as well.
- The NUC acknowledges every valid packet, including duplicates; duplicates are recorded but counted only once in unique-delivery totals. Malformed packets are recorded/rejected without a normal ACK.

**Timing, reliability, and counting:**

- Send **once** per sequence number; there are **no application-level retransmissions**.
- Record each send against the ESP32's **monotonic** timestamp. On matching ACK, calculate `RTT = ACK receive time - original send time`.
- Start with a **provisional 2000 ms ACK timeout** to be assessed in hardware pilots. A timed-out message stays identifiable; if a matching ACK arrives later, record it as a **late ACK** rather than reclassifying the original timeout without a trace.
- Sender log: run ID, class, sequence, send time, ACK/timeout/late status, RTT when available. NUC log: run ID, class, sequence, source endpoint, receipt time, and duplicate/parse status. Exact CSV column names and how logs are exported will be defined in the testing document.
- **Delivery ratio:** count unique valid sequences recorded by the NUC against actual sender-generated sequences. **ACK success:** evaluate separately from receiver delivery. An unacknowledged message may have reached the NUC.
- RTT includes the return path and is **not** a measured one-way delay. Never subtract timestamps from different, unsynchronised devices.

**Interoperability check before integration:** generate one padded Critical message, verify its 128-byte length and parsed fields on the NUC, return the unpadded matching ACK, and observe one ESP32 RTT. Repeat with a 256-byte Routine message. Check a duplicate, an invalid packet, and a deliberately delayed ACK before formal measurements.

The shared protocol is now specified for implementation; the proposed NUC IP address and provisional timeout still require physical verification.

## Bottleneck and QoS boundary

Apply Linux `tc` on the **Pi Ethernet egress** (expected `eth0`). Use the same aggregate capacity limit for the comparable scenarios:

- **Baseline:** no Bulk load; common FIFO treatment at the test bottleneck.
- **Congested FIFO:** Bulk load on; common FIFO treatment at the same limit.
- **Congested classified QoS:** same offered traffic and capacity; classify traffic into Critical, Routine, and Bulk classes.

The planned fixed-QoS approach uses Hierarchical Token Bucket (`HTB`) classes and DSCP-based classification. Exact class rates, queue limits, traffic generation rates, run durations, counters, and comparison methodology belong in [the experiment plan](experiment-plan.md), not in this architecture file.

**Scope limitation:** packets already crossed Wi-Fi before the Pi applies Ethernet-output QoS. Results can support a controlled gateway-QoS claim, **not** claims of Wi-Fi airtime priority, general emergency reliability, or implemented adaptive QoS. Wi-Fi Multimedia (`WMM`) requires separate investigation.

## Implementation order and evidence

1. **Connect the hardware:** Pi Ethernet to NUC; establish the two subnets, Wi-Fi AP, forwarding, return routing, and reachability.
2. **Prove one message path:** ESP32 Critical → Pi → NUC → ACK → ESP32; verify the route and log a sequence number and RTT.
3. **Add other sources:** Routine ESP32 and MacBook `iperf3`; verify the receiver handles all streams.
4. **Introduce the bottleneck:** verify the Pi Ethernet egress is the constrained link and collect queue/throughput evidence.
5. **Compare FIFO vs classified QoS:** verify class counters, record measurements, and follow the repeat procedure in the experiment plan.

**Not yet verified:** AP capability/configuration on the chosen Pi OS, complete packet path, actual DSCP markings, achievable competing load, bottleneck location, or real QoS improvements. Record hardware findings rather than presenting planned behaviour as results.
