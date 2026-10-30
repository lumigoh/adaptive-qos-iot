# Kickoff: Kubuntu NUC UDP Receiver and Logger

**Repository:** https://github.com/lumigoh/adaptive-qos-iot  
**Branch:** `feat/nuc-receiver`

## Objective

Build the UDP receiver, acknowledgement service, and experiment logger running on Lumi's housemate's Kubuntu NUC.

The NUC connects to the Raspberry Pi 5 through Ethernet.

## Project specifications

Read `README.md`, `AGENTS.md`, `docs/architecture.md`, and `docs/testing.md`.

The receiver must eventually:
- Use the proposed address `192.168.60.2`.
- Listen for ESP32 UDP messages on port `5001`.
- Parse and validate the ASCII `Q1` protocol.
- Accept exactly 128-byte Critical and 256-byte Routine messages.
- Return matching, unpadded `A1` acknowledgements to the originating IP and UDP port.
- Identify valid, malformed, and duplicate messages.
- Track unique delivery independently from ACK success.
- Produce the receiver CSV fields specified in `docs/testing.md`.
- Support experimental run identifiers and result collection.
- Run an `iperf3` server on port `5201` for MacBook Bulk traffic.

Prefer Python's standard library for receiver code. Avoid unnecessary frameworks or databases.

## Development approach

Develop and verify the receiver independently before hardware integration.

1. Inspect the repository and confirm the specified UDP protocol.
2. **First coding milestone:** implement a minimal UDP listener that receives one correctly formatted message and returns its matching ACK.
3. Verify this locally using a small UDP test client.
4. Stop and report the implementation and test results.

Later milestones should add validation, duplicates, logging, run management, and `iperf3` integration.

## Constraints

- Keep the UDP interface fully compatible with the ESP32 components.
- Do not infer sender-side loss from missing ACKs.
- Do not calculate one-way delay by subtracting timestamps from different devices.
- Do not change NUC networking or firewall settings without checking existing configuration and explaining potential disruption.
- Keep raw results and never generate synthetic measurements presented as hardware evidence.

Work incrementally on the feature branch. Explain each change and its verification. Do not automatically commit, push, or merge without my request.

**Start with the smallest working receiver and a local test.**
