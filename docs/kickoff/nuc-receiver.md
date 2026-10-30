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

## Development approach — software first

The NUC is not required for initial development. Build the **same receiver application** used later on the NUC, with a configurable bind address (`127.0.0.1` for local tests; the planned NUC address for physical integration).

1. Inspect the repository and confirm the specified `Q1`/`A1` protocol.
2. **First coding milestone:** implement a minimal Python UDP listener that receives one valid 128-byte Critical message and returns the matching **unpadded** ACK to its originating address/port.
3. Verify on `127.0.0.1` with a small independent UDP test client; do not require the Pi, ESP32, or NUC to be online.
4. Stop and report changes, actual local tests, and hardware checks still pending.

Later milestones: Routine 256-byte messages, strict validation, duplicate handling, CSV logging, coordinated run control, and local `iperf3` testing. **Do not invent a separate run-start protocol**: agree on the trigger/transport with the ESP32 and Bulk developers before implementing timed run control (see `docs/testing.md`). Physical Ethernet routing, remote-client ACKs, and performance must be verified separately on hardware.

## Constraints

- Keep the UDP interface fully compatible with the ESP32 components.
- Do not infer sender-side loss from missing ACKs.
- Do not calculate one-way delay by subtracting timestamps from different devices.
- Do not change NUC networking or firewall settings without checking existing configuration and explaining potential disruption.
- Store local/synthetic test output outside `results/<run_id>/`, which is reserved for hardware experiment data. Keep raw results and never present synthetic measurements as hardware evidence.

Work incrementally on the feature branch. Explain each change and its verification. Do not automatically commit, push, or merge without my request.

**Start with the smallest working receiver and a local test.**
