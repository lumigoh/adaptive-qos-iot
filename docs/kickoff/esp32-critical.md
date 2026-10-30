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

## Development approach — software first

Physical ESP32 access is limited. Work alongside me in small, testable increments; **do not wait for the device before developing protocol logic**.

1. Inspect the repository and identify what ESP32 model/toolchain information is known or missing. Avoid choosing a board-specific setup without evidence.
2. Establish a minimal shared firmware structure so Critical and Routine use the same protocol/ACK logic and differ by configuration.
3. **First coding milestone:** implement a `Q1` Critical packet formatter and a **host-side test** checking field contents, ASCII space padding, and exactly 128 bytes.
4. If the target model/toolchain is known and available, compile the firmware; otherwise record the pending compile check and a straightforward command to run later.
5. Stop, explain the changes and test evidence, and wait for review.

Later milestones: ACK matching/timeout logic with host tests, periodic profiles, sender logging, then device Wi-Fi/UDP integration. Hardware-only verification (Wi-Fi, TOS/DSCP, timing and actual RTT) remains pending until a device is available.

Keep host test doubles separate from production firmware and do not report simulated network behaviour as ESP32 hardware evidence.

Avoid unnecessary abstractions, dependencies, duplicate firmware, and unrelated changes. Keep Wi-Fi credentials out of Git.

Work on the feature branch. Do not automatically commit, push, open a PR, or merge without my request.

**Begin by inspecting the repository and tackling only the first small milestone.**
