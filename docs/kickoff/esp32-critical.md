# Kickoff: ESP32 Critical Traffic Generator

**Repository:** https://github.com/lumigoh/adaptive-qos-iot  
**Branch:** `feat/esp32-critical`

## Objective

Develop the Critical traffic generator for our university Adaptive QoS project.

This component should establish a **shared, configurable ESP32 firmware foundation** that the Routine traffic generator can reuse.

## Project specifications

Read `README.md`, `AGENTS.md`, `docs/architecture.md`, and `docs/testing.md`. Treat these as the source of truth.

The Critical ESP32 must eventually:
- Connect to the Raspberry Pi 5 Wi-Fi AP.
- Send 128-byte UDP messages at 5 messages/second.
- Mark packets DSCP `CS5` (TOS `0xA0`).
- Follow the ASCII `Q1` message format, including exact payload padding.
- Receive and validate `A1` acknowledgements from the NUC.
- Measure RTT using its own monotonic clock.
- Track sequences, ACK timeouts, late replies, and experiment phases.
- Produce the sender logs specified in `docs/testing.md`.

Do not implement application-level retransmissions.

## Development approach

Work alongside me in small increments.

1. Inspect the repository and identify the ESP32 model/toolchain requirements. Do not assume either is already configured.
2. Establish the smallest sensible shared firmware structure.
3. **First coding milestone:** generate a correctly formatted 128-byte Critical message and verify its contents and size.
4. Stop, explain the changes, and show how they were verified.

Subsequent milestones will cover Wi-Fi, UDP transmission, acknowledgements, periodic generation, and logging.

If hardware access is unavailable, perform only supported local checks and give me the commands or steps needed for physical verification.

Avoid unnecessary abstractions, dependencies, duplicate firmware, and unrelated changes. Keep Wi-Fi credentials out of Git.

Work on the feature branch. Do not automatically commit, push, open a PR, or merge without my request.

**Begin by inspecting the repository and tackling only the first small milestone.**
