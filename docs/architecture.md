# Hardware architecture

## Goal and scope

Build a small Wi-Fi IoT network and compare how a Raspberry Pi 5 gateway handles Critical, Routine, and Bulk traffic when its **Ethernet output** becomes congested. The primary experiment tests **fixed gateway QoS**, not adaptive control or Wi-Fi airtime prioritisation.

```text
ESP32 (Critical) ─┐
ESP32 (Routine) ──┼── Wi-Fi ──> Raspberry Pi 5 ── Ethernet ──> Kubuntu NUC
Laptop (Bulk) ────┘             AP / QoS gateway                Receiver / logger
                               (shaping on eth0 egress)
```

All three senders connect to the Pi's Wi-Fi access point (AP). The Pi **routes**, rather than locally receives, the test traffic. The NUC receives the traffic, sends acknowledgements to ESP32 devices, and runs the `iperf3` server.

**Why the separate receiver matters:** packets sent directly to a service on the Pi would not traverse its Ethernet egress queue, so they could not demonstrate the intended gateway bottleneck.

The housemate's **Kubuntu NUC is the preferred wired receiver**, subject to availability. A macOS laptop with a USB-C Ethernet adapter is a viable substitute. A physical Ethernet cable is required between the Pi and receiver.

## Components to implement

| Component | Device | Responsibility |
|---|---|---|
| Critical traffic generator | ESP32 | Generate alert-like UDP messages, apply Critical marking, track sequence numbers and acknowledgements |
| Routine traffic generator | ESP32 | Generate periodic telemetry UDP messages, apply Routine marking, track sequence numbers and acknowledgements |
| Bulk traffic generator | Wi-Fi laptop | Generate configurable competing UDP traffic using `iperf3` |
| AP, router, and QoS gateway | Raspberry Pi 5 | Provide Wi-Fi access, route packets to Ethernet, create the controlled output bottleneck, select FIFO or classified QoS |
| Receiver, ACKs, and logging | Separate Kubuntu NUC | Receive and identify ESP32 packets, send UDP acknowledgements, run the `iperf3` server, collect results |

The two ESP32 roles may share one configurable firmware codebase; the two devices run different traffic profiles.

## Network and interfaces

**Proposed IPv4 plan** — based on the current project design, to confirm during physical setup:

| Network/interface | Proposed address | Purpose |
|---|---|---|
| Pi Wi-Fi AP (typically `wlan0`) | `192.168.50.1/24` | Gateway for ESP32 devices and Bulk laptop |
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
4. Independently, the laptop sends Bulk UDP traffic through the same Pi Ethernet output to the NUC's `iperf3` server.

**Proposed Differentiated Services Code Point (DSCP) markings:**

| Traffic | Marking | Sender |
|---|---|---|
| Critical | `CS5` | ESP32 Critical |
| Routine | `CS0` | ESP32 Routine |
| Bulk | `CS1` | Laptop `iperf3`; explicitly configure the marking |

Keep these markings **unchanged across FIFO and classified-QoS scenarios** so the experiment changes gateway scheduling, not the senders' Wi-Fi treatment. The marks and their mapping to Linux `tc` classes must be verified with packet captures/counters.

**Shared packet contract — needs to be finalised before independent sender/receiver implementation:**

- Common UDP destination port for ESP32 messages; `iperf3` uses its own server port.
- Message fields sufficient to identify **run ID, device/class, and sequence number**.
- ACK fields sufficient to match exactly one sent message.
- Packet encoding and exact payload sizes; timeout/retry behaviour; logging column names.

The ESP32 sender, NUC receiver, and logger must agree on that contract. Do not assume separately developed formats will interoperate.

RTT includes the return path; it must not be reported as measured one-way delay. A missing ACK is not, by itself, proof that the forward packet was lost.

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
3. **Add other sources:** Routine ESP32 and laptop `iperf3`; verify the receiver handles all streams.
4. **Introduce the bottleneck:** verify the Pi Ethernet egress is the constrained link and collect queue/throughput evidence.
5. **Compare FIFO vs classified QoS:** verify class counters, record measurements, and follow the repeat procedure in the experiment plan.

**Not yet verified:** AP capability/configuration on the chosen Pi OS, complete packet path, actual DSCP markings, achievable competing load, bottleneck location, or real QoS improvements. Record hardware findings rather than presenting planned behaviour as results.
