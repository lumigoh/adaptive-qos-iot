# Adaptive QoS for a Wireless IoT Network

A three-person **31275 Mobile Networking** project investigating whether gateway Quality of Service (QoS) can protect critical messages from Wi-Fi-connected Internet of Things (IoT) devices under congestion.

[Group report](https://docs.google.com/document/d/136MyeV2-TDr8ddHiX4-jOOZTiXmzx_QEbfMC_F8kdaE/edit?tab=t.0)

## System

```text
ESP32 A (Critical) ─┐
ESP32 B (Normal) ───┼─ Wi-Fi → Raspberry Pi 5 AP/router → Ethernet → wired receiver
Bulk laptop ────────┘
```

The Raspberry Pi routes traffic to a separate wired receiver. Linux `tc` will create a controlled bottleneck on the Pi's Ethernet output and apply traffic classification and scheduling. The bulk laptop uses `iperf3` to generate competing traffic; the receiver logs messages and sends acknowledgements for round-trip time (RTT) measurements.

The primary experiment compares **Baseline**, **Congested FIFO**, and **Congested classified QoS**. We first test fixed gateway QoS; adaptive control and Wi-Fi Multimedia (WMM) are possible later investigations. Results from the Ethernet-output bottleneck do **not** establish Wi-Fi airtime prioritisation.

## Components and responsibilities

| Component | Focus |
|---|---|
| ESP32 devices — Person A | Generate Critical and Normal messages, set traffic markings, measure acknowledgements |
| Raspberry Pi — Person B | Wi-Fi AP/router, Ethernet bottleneck, `tc` QoS configuration |
| Wired receiver / testing — Person C | Receive and acknowledge messages, run `iperf3`, collect results |

All three team members help integrate the devices, interpret results, and write the report.

## Current status

- **Presentation prototype:** a working interactive **simulation** in [`prototype/`](prototype/). It illustrates fixed QoS, but is not a hardware implementation or experimental evidence.
- **Physical system:** end-to-end operation and QoS measurements have not yet been verified in the repository.
- **Next milestone:** send a message from an ESP32 through the Pi to the wired receiver and receive an acknowledgement. Then introduce controlled congestion and compare FIFO with fixed QoS.

### Run the presentation prototype

```sh
cd prototype
npm ci
npm run dev
```

See [`prototype/README.md`](prototype/README.md) for prerequisites, demo instructions, and validation details.

## Documentation

- [Architecture](docs/architecture.md) — network topology, device roles, packet path, and interfaces.
- [Experiment plan](docs/experiment-plan.md) — scenarios, measurement approach, and settings to finalise.
- [Decision log](docs/decisions.md) — existing technical proposals; useful decisions will be folded into the relevant documents during cleanup.

## Collaboration

Work on individual component branches and merge working changes into `main`. Coordinate shared interfaces (addresses, ports, packet formats, acknowledgements, and traffic settings) before implementing incompatible assumptions. Keep setup, technical specifications, and testing instructions in the README or `docs/`.
