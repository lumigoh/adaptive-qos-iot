# Adaptive QoS for a Wireless IoT Network

A three-person **31275 Mobile Networking** project investigating whether gateway Quality of Service (QoS) can protect critical messages from Wi-Fi-connected Internet of Things (IoT) devices under congestion.

[Group report](https://docs.google.com/document/d/136MyeV2-TDr8ddHiX4-jOOZTiXmzx_QEbfMC_F8kdaE/edit?tab=t.0)

## System

```text
ESP32 (Critical) ─┐
ESP32 (Routine) ──┼─ Wi-Fi → Raspberry Pi 5 AP/QoS gateway → Ethernet → wired receiver
MacBook (Bulk) ───┘                                      (Kubuntu NUC receiver)
```

The gateway Raspberry Pi routes Wi-Fi traffic to a **separate Ethernet-connected Kubuntu NUC borrowed from Lumi's housemate**. Linux `tc` will create a controlled bottleneck on the gateway's Ethernet output and apply traffic classification and scheduling. The MacBook connects over Wi-Fi and runs `iperf3` to generate competing traffic; the NUC runs the corresponding server, logs messages, and sends acknowledgements for round-trip time (RTT) measurements.

The primary experiment compares **Baseline**, **Congested FIFO**, and **Congested classified QoS**. We first test fixed gateway QoS; adaptive control and Wi-Fi Multimedia (WMM) are possible later investigations. Results from the Ethernet-output bottleneck do **not** establish Wi-Fi airtime prioritisation.

## Components to build

| Component | Hardware | Work needed |
|---|---|---|
| [Critical traffic generator](docs/kickoff/esp32-critical.md) | ESP32 | Send time-sensitive alert-style messages with identifiers and traffic markings; record acknowledgements and RTT. |
| [Routine traffic generator](docs/kickoff/esp32-routine.md) | ESP32 | Send periodic telemetry messages with identifiers and traffic markings; record acknowledgements and RTT. |
| [Bulk traffic generator](docs/kickoff/macbook-bulk.md) | MacBook (Wi-Fi) | Run `iperf3` UDP traffic at a configurable rate to create competing load. |
| [QoS gateway and access point](docs/kickoff/pi-gateway.md) | Raspberry Pi 5 | Host the Wi-Fi network, route to Ethernet, configure a controlled `tc` bottleneck, and apply FIFO or classified QoS. |
| [Wired receiver, testing, and logging](docs/kickoff/nuc-receiver.md) | Kubuntu NUC (Lumi's housemate) | Receive ESP32 messages, send acknowledgements, run the `iperf3` server, and collect experiment logs and results. |

These are **implementation components, not assigned owners**. Divide coding between teammates as convenient, then integrate and test the full system together. The gateway and receiver must be physically separate and connected by Ethernet for the gateway egress experiment.

## Development approach — software first

**Physical hardware access is limited.** Build and verify as much as possible locally before scheduling a hardware session:

1. **Receiver:** implement the Python UDP/ACK server and test it with a localhost client using the real `Q1`/`A1` protocol.
2. **ESP32 firmware:** implement one shared firmware codebase for Critical and Routine profiles; test packet/ACK logic on a host and compile for the selected board when its toolchain is known.
3. **Pi gateway:** prepare OS-independent `tc` scenario scripts, inspection and rollback steps using configurable interfaces; defer OS-specific AP/routing configuration until the Pi OS and networking services are confirmed.
4. **MacBook Bulk:** verify local `iperf3` client/server commands and JSON output before pointing them at the NUC.

Suggested team split: **ESP32 firmware**, **Pi gateway**, and **NUC receiver + MacBook Bulk**. The [component kickoff prompts](docs/kickoff/) guide small, independently verifiable milestones.

**Local tests are software checks, not hardware or QoS performance evidence.** On-device routing, Wi-Fi behaviour, packet markings, the egress bottleneck, and formal experiment results still require hardware pilots.

## Current status

- **Presentation prototype:** a working interactive **simulation** in [`prototype/`](prototype/). It illustrates fixed QoS, but is not a hardware implementation or experimental evidence.
- **Physical system:** end-to-end operation and QoS measurements have not yet been verified in the repository.
- **Next milestone (software):** implement and locally test the UDP receiver/ACKs, shared ESP32 firmware, gateway configuration scripts, and Bulk commands.
- **First hardware milestone (later):** verify ESP32 → Pi → NUC → ACK, then validate the controlled bottleneck and QoS scenarios.

### Run the presentation prototype

```sh
cd prototype
npm ci
npm run dev
```

See [`prototype/README.md`](prototype/README.md) for prerequisites, demo instructions, and validation details.

## Documentation

Project documentation lives in [`docs/`](docs/), including any new references added later.

- [Architecture](docs/architecture.md) — topology, addresses, device interfaces, and UDP message/ACK protocol.
- [Testing specification](docs/testing.md) — traffic rates, QoS settings, scenarios, logging, and experiment procedure.
- [Component kickoff prompts](docs/kickoff/) — small, step-by-step starting tasks for AI coding agents (also linked from the component table above).

The architecture and testing documents define the shared technical specifications; kickoff prompts guide implementation.

## Collaboration

Work on individual component branches and merge working changes into `main`. Coordinate shared interfaces (addresses, ports, packet formats, acknowledgements, and traffic settings) before implementing incompatible assumptions. Keep setup, technical specifications, and testing instructions in the README or `docs/`.
